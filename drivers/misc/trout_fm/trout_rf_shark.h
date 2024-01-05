#ifndef _TROUT_FM_RF_SHARK_H_
#define _TROUT_FM_RF_SHARK_H_


#define SYS_REG_PAD_MIN			0
#define SYS_REG_PAD_IISDO		0
#define SYS_REG_PAD_IISCLK		1
#define SYS_REG_PAD_IISLRCK		2
#define SYS_REG_PAD_PTEST		3
#define SYS_REG_PAD_GINT		4
#define SYS_REG_PAD_XTLEN		5
#define SYS_REG_PAD_RESETN		6
#define SYS_REG_PAD_GPIO		7
#define SYS_REG_PAD_U0CTS		8
#define SYS_REG_PAD_U0RTS		9
#define SYS_REG_PAD_U0RXD		0xA
#define SYS_REG_PAD_U0TXD		0xB
#define SYS_REG_PAD_CLK_32K		0xC
#define SYS_REG_PAD_SD_CLK		0xD
#define SYS_REG_PAD_SD_CMD		0xE
#define SYS_REG_PAD_SD_D0		0xF
#define SYS_REG_PAD_SD_D1		0x10
#define SYS_REG_PAD_SD_D2		0x11
#define SYS_REG_PAD_SD_D3		0x12
#define SYS_REG_PAD_IISDI		0x13
#define SYS_REG_PAD_MAX			SYS_REG_PAD_IISDI

#define	SYS_REG_FUNC_LOWSPEED		0x40
#define	SYS_REG_CLK_CTRL0			0x43
#define	SYS_REG_CLK_CTRL1			0x44

#define	WIFI_SLEEP_CLK_SEL			BIT_16
#define	SPI_XTL_AUTOPD_EN			BIT_13
#define	SPI_XTL_FORCE_ON			BIT_12
#define	SPI_FORCE_SLEEP				BIT_11
#define	SPI_FM_SLEEP				BIT_10

#define	SYS_REG_POW_CTRL			0x45

#define	PMU_PD_INT					BIT_0
#define	LDO_ANA_PD					BIT_4
#define	PMU_PD_INT_AUTO				BIT_8
#define	LDO_ANA_PD_AUTO				BIT_10

#define	SYS_REG_GPIO_IN				0x46
#define	SYS_REG_GPIO_OUT			0x47
#define	SYS_REG_GPIO_OE				0x48
#define	SYS_REG_GPIO_INEN			0x49
#define	SYS_REG_TEST_SEL			0x4D
#define	SYS_REG_WIFI_CFG			0x4E
#define	SYS_REG_ADDA_FORCE			0x50
#define	SYS_REG_WIFI_SOFT_RST		0x51
#define	SYS_REG_BIST_CFG			0x52
#define	SYS_REG_DACCONST_CFG		0x53
#define	SYS_REG_UPSAMPLE_CFG		0x54
#define	SYS_REG_IQ_SWAP				0x55
#define	SYS_REG_ROSYS_REG_BIST_RESULT0	0x56
#define	SYS_REG_ROSYS_REG_BIST_RESULT1	0x57
#define	SYS_REG_ANT_MODE				0x58
#define	SYS_REG_HOST2ARSYS_REG_INFO0	0x5A
#define	SYS_REG_HOST2ARSYS_REG_INFO1	0x5B
#define	SYS_REG_HOST2ARSYS_REG_INFO2	0x5C
#define	SYS_REG_HOST2ARSYS_REG_INFO3	0x5D
#define	SYS_REG_INFO0_FROM_ARM			0x5E
#define	SYS_REG_INFO1_FROM_ARM			0x5F
#define	SYS_REG_INFO2_FROM_ARM			0x60
#define	SYS_REG_INFO3_FROM_ARM			0x61
#define	SYS_REG_WIFIAD_CFG				0x63
#define	SYS_REG_BTAD_CFG				0x65

#define	SYS_REG_TROUT_FAIL_H1			0x6A

#define	SYS_REG_BWDAC_CFG0				0x6E
#define	SYS_REG_BWDAC_CFG1				0x6F
#define	SYS_REG_BWDAC_CFG2				0x70
#define	SYS_REG_BWDAC_STS0				0x71
#define	SYS_REG_BWDAC_STS1				0x72

#define	SYS_REG_ADDA_TEST_CFG			0xC7
#define	SYS_REG_FPGA_DEBUG_CFG			0xC8

#ifdef CONFIG_FM_SEEK_STEP_50KHZ
#define MAX_FM_FREQ	10800
#define MIN_FM_FREQ	8750
#else
#define MAX_FM_FREQ	1080
#define MIN_FM_FREQ	875
#endif
#define FM_CTL_STI_MODE_NORMAL		0x0
#define	FM_CTL_STI_MODE_SEEK		    0x1
#define	FM_CTL_STI_MODE_TUNE		    0x2

/*reg base start by renfeng*/

#define SHARK_FM_REG_BASE               0x40270000

#define	FM_REG_FM_CTRL                  (SHARK_FM_REG_BASE + 0x0000)
#define	FM_REG_FM_EN                    (SHARK_FM_REG_BASE + 0x0004)
#define	FM_REG_RF_INIT_GAIN            (SHARK_FM_REG_BASE + 0x0008)
#define	FM_REG_CHAN                     (SHARK_FM_REG_BASE + 0x000C)
#define	FM_REG_AGC_TBL_CLK              (SHARK_FM_REG_BASE + 0x0010)
#define	FM_REG_SEEK_LOOP                (SHARK_FM_REG_BASE + 0x0014)
#define	FM_REG_FMCTL_STI                (SHARK_FM_REG_BASE + 0x0018)
#define	FM_REG_BAND_LMT                 (SHARK_FM_REG_BASE + 0x001C)
#define	FM_REG_BAND_SPACE               (SHARK_FM_REG_BASE + 0x0020)
#define	FM_REG_RF_CTL                   (SHARK_FM_REG_BASE + 0x0024)
/*#ifdef TROUT_VER_II*/
#define FM_REG_RF_CTL1                  (SHARK_FM_REG_BASE + 0x0028)
/*#else
#define FM_REG_ADC_BITMAP               (SHARK_FM_REG_BASE + 0x0004)
#endif*/
#define	FM_REG_INT_EN                   (SHARK_FM_REG_BASE + 0x0030)
#define	FM_REG_INT_CLR                  (SHARK_FM_REG_BASE + 0x0034)
#define	FM_REG_INT_STS                  (SHARK_FM_REG_BASE + 0x0038)
#define	FM_REG_ADC_INFCTRL              (SHARK_FM_REG_BASE + 0x003C)
#define	FM_REG_SEEK_CH_TH               (SHARK_FM_REG_BASE + 0x0040)
#define	FM_REG_AGC_TBL_RFRSSI0          (SHARK_FM_REG_BASE + 0x0044)
#define	FM_REG_AGC_TBL_RFRSSI1          (SHARK_FM_REG_BASE + 0x0048)
#define	FM_REG_AGC_TBL_WBRSSI           (SHARK_FM_REG_BASE + 0x004C)
#define	FM_REG_AGC_IDX_TH               (SHARK_FM_REG_BASE + 0x0050)
#define	FM_REG_AGC_RSSI_TH              (SHARK_FM_REG_BASE + 0x0054)
#define	FM_REG_SEEK_ADPS                (SHARK_FM_REG_BASE + 0x0058)
#define	FM_REG_STER_PWR                 (SHARK_FM_REG_BASE + 0x005C)
#define	FM_REG_AGC_CTRL                 (SHARK_FM_REG_BASE + 0x0060)
#define	FM_REG_AGC_ITV_TH               (SHARK_FM_REG_BASE + 0x0064)
#define	FM_REG_AGC_ADPS0                (SHARK_FM_REG_BASE + 0x0068)
#define	FM_REG_AGC_ADPS1                (SHARK_FM_REG_BASE + 0x006C)
#define	FM_REG_PDP_TH                   (SHARK_FM_REG_BASE + 0x0070)
#define	FM_REG_PDP_DEV                  (SHARK_FM_REG_BASE + 0x0074)
#define	FM_REG_ADP_ST_CONF              (SHARK_FM_REG_BASE + 0x0078)
#define	FM_REG_ADP_LPF_CONF             (SHARK_FM_REG_BASE + 0x007C)
#define	FM_REG_DEPHA_SCAL               (SHARK_FM_REG_BASE + 0x0080)
#define	FM_REG_HW_MUTE		 (SHARK_FM_REG_BASE + 0x0084)
#define	FM_REG_SW_MUTE		 (SHARK_FM_REG_BASE + 0x0088)
#define	FM_REG_UPD_CTRL		 (SHARK_FM_REG_BASE + 0x008C)
#define	FM_REG_AUD_BLD0		 (SHARK_FM_REG_BASE + 0x0090)
#define	FM_REG_AUD_BLD1		 (SHARK_FM_REG_BASE + 0x0094)
#define	FM_REG_AGC_HYS		 (SHARK_FM_REG_BASE + 0x0098)
#define	FM_REG_MONO_PWR		 (SHARK_FM_REG_BASE + 0x009C)
#define	FM_REG_RF_CFG_DLY    (SHARK_FM_REG_BASE + 0x00A0)
#define	FM_REG_AGC_TBL_STS	 (SHARK_FM_REG_BASE + 0x00A4)
#define	FM_REG_ADP_BIT_SFT	 (SHARK_FM_REG_BASE + 0x00A8)
#define	FM_REG_SEEK_CNT		 (SHARK_FM_REG_BASE + 0x00AC)
#define	FM_REG_RSSI_STS		 (SHARK_FM_REG_BASE + 0x00B0)
#define	FM_REG_CHAN_FREQ_STS	 (SHARK_FM_REG_BASE + 0x00B8)
#define	FM_REG_FREQ_OFF_STS	     (SHARK_FM_REG_BASE + 0x00BC)
#define	FM_REG_INPWR_STS	     (SHARK_FM_REG_BASE + 0x00C0)
#define	FM_REG_RF_RSSI_STS	     (SHARK_FM_REG_BASE + 0x00C4)
#define	FM_REG_NO_LPF_STS	     (SHARK_FM_REG_BASE + 0x00C8)
#define	FM_REG_SMUTE_STS	     (SHARK_FM_REG_BASE + 0x00CC)
#define	FM_REG_WBRSSI_STS	     (SHARK_FM_REG_BASE + 0x00D0)
#define	FM_REG_AGC_BITS_TBL0	 (SHARK_FM_REG_BASE + 0x0100)
#define	FM_REG_AGC_BITS_TBL1	 (SHARK_FM_REG_BASE + 0x0104)
#define	FM_REG_AGC_BITS_TBL2	 (SHARK_FM_REG_BASE + 0x0108)
#define	FM_REG_AGC_BITS_TBL3	 (SHARK_FM_REG_BASE + 0x010C)
#define	FM_REG_AGC_BITS_TBL4	 (SHARK_FM_REG_BASE + 0x0110)
#define	FM_REG_AGC_BITS_TBL5	 (SHARK_FM_REG_BASE + 0x0114)
#define	FM_REG_AGC_BITS_TBL6	 (SHARK_FM_REG_BASE + 0x0118)
#define	FM_REG_AGC_BITS_TBL7	 (SHARK_FM_REG_BASE + 0x011C)
#define	FM_REG_AGC_BITS_TBL8	 (SHARK_FM_REG_BASE + 0x0120)
#define	FM_REG_AGC_BITS_TBL9	 (SHARK_FM_REG_BASE + 0x0124)
#define	FM_REG_AGC_BITS_TBL10	 (SHARK_FM_REG_BASE + 0x0128)
#define	FM_REG_AGC_BITS_TBL11	 (SHARK_FM_REG_BASE + 0x012C)
#define	FM_REG_AGC_BITS_TBL12	 (SHARK_FM_REG_BASE + 0x0130)
#define	FM_REG_AGC_BITS_TBL13	 (SHARK_FM_REG_BASE + 0x0134)
#define	FM_REG_AGC_BITS_TBL14	 (SHARK_FM_REG_BASE + 0x0138)
#define	FM_REG_AGC_BITS_TBL15	 (SHARK_FM_REG_BASE + 0x013C)
#define	FM_REG_AGC_BITS_TBL16	 (SHARK_FM_REG_BASE + 0x0140)
#define	FM_REG_AGC_BITS_TBL17	 (SHARK_FM_REG_BASE + 0x0144)
#define	FM_REG_AGC_VAL_TBL0		 (SHARK_FM_REG_BASE + 0x0200)
#define	FM_REG_AGC_VAL_TBL1		 (SHARK_FM_REG_BASE + 0x0204)
#define	FM_REG_AGC_VAL_TBL2		 (SHARK_FM_REG_BASE + 0x0208)
#define	FM_REG_AGC_VAL_TBL3		 (SHARK_FM_REG_BASE + 0x020C)
#define	FM_REG_AGC_VAL_TBL4		 (SHARK_FM_REG_BASE + 0x0210)
#define	FM_REG_AGC_VAL_TBL5		 (SHARK_FM_REG_BASE + 0x0214)
#define	FM_REG_AGC_VAL_TBL6		 (SHARK_FM_REG_BASE + 0x0218)
#define	FM_REG_AGC_VAL_TBL7		 (SHARK_FM_REG_BASE + 0x021C)
#define	FM_REG_AGC_VAL_TBL8		 (SHARK_FM_REG_BASE + 0x0220)
#define	FM_REG_AGC_VAL_TBL9		 (SHARK_FM_REG_BASE + 0x0224)
#define	FM_REG_AGC_VAL_TBL10	 (SHARK_FM_REG_BASE + 0x0228)
#define	FM_REG_AGC_VAL_TBL11	 (SHARK_FM_REG_BASE + 0x022C)
#define	FM_REG_AGC_VAL_TBL12	 (SHARK_FM_REG_BASE + 0x0230)
#define	FM_REG_AGC_VAL_TBL13	 (SHARK_FM_REG_BASE + 0x0234)
#define	FM_REG_AGC_VAL_TBL14	 (SHARK_FM_REG_BASE + 0x0238)
#define	FM_REG_AGC_VAL_TBL15	 (SHARK_FM_REG_BASE + 0x023C)
#define	FM_REG_AGC_VAL_TBL16	 (SHARK_FM_REG_BASE + 0x0240)
#define	FM_REG_AGC_VAL_TBL17	 (SHARK_FM_REG_BASE + 0x0244)
#define	FM_REG_AGC_BOND_TBL0	 (SHARK_FM_REG_BASE + 0x0300)
#define	FM_REG_AGC_BOND_TBL1	 (SHARK_FM_REG_BASE + 0x0304)
#define	FM_REG_AGC_BOND_TBL2	 (SHARK_FM_REG_BASE + 0x0308)
#define	FM_REG_AGC_BOND_TBL3	 (SHARK_FM_REG_BASE + 0x030C)
#define	FM_REG_AGC_BOND_TBL4	 (SHARK_FM_REG_BASE + 0x0310)
#define	FM_REG_AGC_BOND_TBL5	 (SHARK_FM_REG_BASE + 0x0314)
#define	FM_REG_AGC_BOND_TBL6	 (SHARK_FM_REG_BASE + 0x0318)
#define	FM_REG_AGC_BOND_TBL7	 (SHARK_FM_REG_BASE + 0x031C)
#define	FM_REG_AGC_BOND_TBL8	 (SHARK_FM_REG_BASE + 0x0320)
#define	FM_REG_AGC_BOND_TBL9	 (SHARK_FM_REG_BASE + 0x0324)

#define FM_REG_SPUR_FREQ0       (SHARK_FM_REG_BASE + 0x340) /* new reg*/
#define FM_REG_SPUR_FREQ1       (SHARK_FM_REG_BASE + 0X344) /* new reg*/

#define	FM_REG_AGC_DB_TBL_BEGIN	 (SHARK_FM_REG_BASE + 0x0500)
#define FM_REG_AGC_DB_TBL_END	 (SHARK_FM_REG_BASE + 0x05A0)
#define FM_REG_AGC_DB_TBL_CNT	 \
	((FM_REG_AGC_DB_TBL_END-FM_REG_AGC_DB_TBL_BEGIN)/4 + 1)

#define	FM_REG_SPI_CTL			 (SHARK_FM_REG_BASE + 0x0810)
#define	FM_REG_SPI_WD0			 (SHARK_FM_REG_BASE + 0x0814)
#define FM_REG_SPI_WD1			 (SHARK_FM_REG_BASE + 0x0818)
#define FM_REG_SPI_RD			 (SHARK_FM_REG_BASE + 0x081C)
#define FM_REG_SPI_FIFO_STS		 (SHARK_FM_REG_BASE + 0x0820)

#define SHARK_AON_CLK_BASE_ADDR      0x402D0000
#define SHARK_AON_CLK_FM_CFG         (SHARK_AON_CLK_BASE_ADDR+0x0030)

#define SHARK_APB_BASE_ADDR         0x402E0000
#define SHARK_APB_EB0               (SHARK_APB_BASE_ADDR+0x0000)
#define SHARK_APB_EB1               (SHARK_APB_BASE_ADDR+0x0004)
#define SHARK_APB_RST0              (SHARK_APB_BASE_ADDR+0x0008)
#define SHARK_APB_EB0_SET           (SHARK_APB_BASE_ADDR+0x1000)


#define SHARK_AHB_BASE_ADDR         0x71300000
#define SHARK_AHB_EB                (SHARK_APB_BASE_ADDR+0x0000)
#define SHARK_AHB_RST               (SHARK_APB_BASE_ADDR+0x0004)

#define  PINMAP_ADDR                    0x402A0000
#define  PINMAP_FOR_FMIQ           PINMAP_ADDR
#if defined(CONFIG_DOLPHIN_CHIP_2351)
#define  PINMAP_FOR_IIS0DO        (PINMAP_ADDR + 0x02F4)
#define  PINMAP_FOR_IIS0DI        (PINMAP_ADDR + 0x02F0)
#elif defined(CONFIG_SHARK_CHIP_2351)
#define  PINMAP_FOR_IIS0DO        (PINMAP_ADDR + 0x0404)
#define  PINMAP_FOR_IIS0DI        (PINMAP_ADDR + 0x0408)
#endif



#define  PINMAP_FOR_IQDEBUG    (PINMAP_ADDR + 0x000C)
#define  PINMAP_FOR_TPDP           (PINMAP_ADDR + 0x0390)
#define  PINMAP_FOR_TPDN           (PINMAP_ADDR + 0x039C)
#define  PINMAP_FOR_TPCK           (PINMAP_ADDR + 0x03A0)

#define  SHARK_PMU_BASE_ADDR        0x402B0000
#define  SHARK_PMU_SLEEP_CTRL       (SHARK_PMU_BASE_ADDR+0x00C4)

#define SHARK_FM_MSPI_BASE          0x40070000
#define SHARK_MSPI_CFG0             (SHARK_FM_MSPI_BASE + 0x0000)
#define SHARK_MSPI_CFG1             (SHARK_FM_MSPI_BASE + 0x0004)
#define SHARK_MSPI_CFG2             (SHARK_FM_MSPI_BASE + 0x0008)
#define SHARK_MSPI_MCU_WCMD         (SHARK_FM_MSPI_BASE + 0x000C)
#define SHARK_MSPI_MCU_RCMD         (SHARK_FM_MSPI_BASE + 0x0010)
#define SHARK_MSPI_MCU_RDATA        (SHARK_FM_MSPI_BASE + 0x0014)

#define SHARK_FM_SPI_WRITE(addr, data)	\
	((0UL<<31)|((addr&0x7f)<<24)|((data&0xffff)<<8))
#define SHARK_FM_SPI_READ(addr)			((1UL<<31)|((addr&0x7f)<<24))
#define SPI_ADDR_SELECT 0x402E2140
#if 0
#define BIT_0               0x00000001
#define BIT_1               0x00000002
#define BIT_2               0x00000004
#define BIT_3               0x00000008
#define BIT_4               0x00000010
#define BIT_5               0x00000020
#define BIT_6               0x00000040
#define BIT_7               0x00000080
#define BIT_8               0x00000100
#define BIT_9               0x00000200
#define BIT_10              0x00000400
#define BIT_11              0x00000800
#define BIT_12              0x00001000
#define BIT_13              0x00002000
#define BIT_14              0x00004000
#define BIT_15              0x00008000
#define BIT_16              0x00010000
#define BIT_17              0x00020000
#define BIT_18              0x00040000
#define BIT_19              0x00080000
#define BIT_20              0x00100000
#define BIT_21              0x00200000
#define BIT_22              0x00400000
#define BIT_23              0x00800000
#define BIT_24              0x01000000
#define BIT_25              0x02000000
#define BIT_26              0x04000000
#define BIT_27              0x08000000
#define BIT_28              0x10000000
#define BIT_29              0x20000000
#define BIT_30              0x40000000
#define BIT_31              0x80000000
#endif
/*functions need by shark*/

int trout_fm_en(void);
void __trout_fm_get_status(void);
int trout_fm_dis(void);
int trout_fm_get_frequency(u16 *);

/*typedef unsigned char       BOOLEAN;*/
/*
typedef unsigned char       u328;
typedef unsigned short      u3216;
typedef unsigned long int   u32;
typedef unsigned long int   u3264;
typedef unsigned int        u32;

typedef signed char         int8;
typedef signed short        int16;
typedef signed long int     int32;
*/
/*#define TRUE   1    Boolean true value. */
/*#define FALSE  0    Boolean false value. */

/*
#define EQ ==
#define NEQ !=


#define	LOCAL		static
#define	CONST		const

#define PNULL		0
#define PUBLIC
*/
#define INT_NUM_FM_test 27

struct shark_fm_info_t {
	int		int_happen;
	int		seek_success;
	u32		freq_set;
	u32		freq_seek;
	u32		freq_offset;
	u32		rssi;
	u32		rf_rssi;
	u32		seek_cnt;
	u32		inpwr_sts;
	u32		fm_sts;
	u32      agc_sts;
    /*void	(*irq_handler)(u32);*/
};


struct shark_reg_cfg_t {
	unsigned int reg_addr;
	unsigned int reg_data;
};

void irq_enable(u32 Channel_ID);
void delay(u32 cnt);
void shark_fm_int_en(void);
void shark_fm_int_dis(void);
int shark_write_regs(u32 reg_addr, u32 reg_cnt, u32 *reg_data);
void shark_fm_irq_handler(u32 int_sts);
u32 get_number(void);

char UartGetChNoWait(void);
void Uart_GetString(char *buffer);
u32 str_to_hex(char *string);

u32 g_spi_select = 1; /*0:spi, 1:mspi*/

struct shark_fm_info_t shark_fm_info = {
	1,	/* int_happen;*/
	1,	/*	seek_success;*/
	0,	/*   freq_set;*/
	0,	/*	freq_seek;*/
	0,	/*	freq_offset;*/
	0,	/*	rssi;*/
	0,	/*	rf_rssi;*/
	0,	/*	seek_cnt;*/
	0,	/*	inpwr_sts;*/
	0,	/*	fm_sts;*/
	0,	/*  agc_sts*/
	/*shark_fm_irq_handler,		(*irq_handler)(u32);*/
};

struct shark_reg_cfg_t fm_reg_init_des[] = {
	{FM_REG_FM_CTRL	,		0x00001011},
	/*{FM_REG_FM_CTRL	,			0x00023111},*/
	{FM_REG_FM_EN	,			0x000032EE},
	{FM_REG_CHAN	,			(899-1)*2},
	/*{FM_REG_CHAN	,			899},*/
	{FM_REG_AGC_TBL_CLK,		0x00000000},
	{FM_REG_SEEK_LOOP	,		1080 - 875},
	{FM_REG_FMCTL_STI	,		0x0},
	{FM_REG_BAND_LMT	,		\
	(((1080 - 1) * 2)<<16) | ((875-1)*2)},
#ifdef CONFIG_FM_SEEK_STEP_50KHZ
    {FM_REG_BAND_SPACE	,		0x00000001},
#else	
	{FM_REG_BAND_SPACE	,		0x00000002},
#endif	
	/*{FM_REG_RFREG_ADDR	,	0x0},*/
	{FM_REG_ADC_INFCTRL, 0}, /*0x00067f7f}, new*/
	{FM_REG_SEEK_CH_TH	,		2000},
	{FM_REG_AGC_TBL_RFRSSI0,	0x00020800},
	{FM_REG_AGC_TBL_RFRSSI1,	(31 << 24) | (5 << 16) | (3000)},
	{FM_REG_AGC_TBL_WBRSSI	,	0x00F60C12},
	{FM_REG_AGC_IDX_TH	,		0x00020407},
	{FM_REG_AGC_RSSI_TH	,		0x00F600D8},
	{FM_REG_SEEK_ADPS	,		0x0CD80000},
	{FM_REG_STER_PWR	,		0x000001A6},
	{FM_REG_MONO_PWR	,		0x019C0192},
	{FM_REG_AGC_CTRL	,		0x09040120},
	{FM_REG_AGC_ITV_TH	,		0x00FE00EE},
	{FM_REG_AGC_ADPS0	,		0xFDF70009},
	{FM_REG_AGC_ADPS1	,		0xFEF80003},
	{FM_REG_PDP_TH	,			400},
	{FM_REG_PDP_DEV	,			120},
	{FM_REG_ADP_ST_CONF	,		0x200501BA},
	{FM_REG_ADP_LPF_CONF	,	2},
	{FM_REG_DEPHA_SCAL	,		5},
	{FM_REG_HW_MUTE	,			0},
	{FM_REG_SW_MUTE	,			0x01A601A1},
	{FM_REG_UPD_CTRL	,		0x01013356},
	{FM_REG_AUD_BLD0	,		0x01AD01A1},
	{FM_REG_AUD_BLD1	,		0x81A901A5},
	/*{FM_REG_AUD_BLD1	,	0x01A901A5},*/
	{FM_REG_AGC_HYS	,			0x00000202},
	{FM_REG_RF_CFG_DLY	,		20000},
	{FM_REG_RF_INIT_GAIN,	0x0303},
	{FM_REG_AGC_BITS_TBL0	,	0x0000},
	{FM_REG_AGC_BITS_TBL1	,	0x0001},
	{FM_REG_AGC_BITS_TBL2	,	0x0301},
	{FM_REG_AGC_BITS_TBL3	,	0x0302},
	{FM_REG_AGC_BITS_TBL4	,	0x0303},
	{FM_REG_AGC_BITS_TBL5	,	0x0313},
	{FM_REG_AGC_BITS_TBL6	,	0x0323},
	{FM_REG_AGC_BITS_TBL7	,	0x0333},
	{FM_REG_AGC_BITS_TBL8	,	0x0335},
	{FM_REG_AGC_BITS_TBL9	,	0x0000},
	{FM_REG_AGC_BITS_TBL10	,	0x0001},
	{FM_REG_AGC_BITS_TBL11	,	0x0301},
	{FM_REG_AGC_BITS_TBL12	,	0x0302},
	{FM_REG_AGC_BITS_TBL13	,	0x0303},
	{FM_REG_AGC_BITS_TBL14	,	0x0313},
	{FM_REG_AGC_BITS_TBL15	,	0x0323},
	{FM_REG_AGC_BITS_TBL16	,	0x0333},
	{FM_REG_AGC_BITS_TBL17	,	0x0335},
	{FM_REG_AGC_VAL_TBL0	,	7},
	{FM_REG_AGC_VAL_TBL1	,	21},
	{FM_REG_AGC_VAL_TBL2	,	29},
	{FM_REG_AGC_VAL_TBL3	,	35},
	{FM_REG_AGC_VAL_TBL4	,	41},
	{FM_REG_AGC_VAL_TBL5	,	47},
	{FM_REG_AGC_VAL_TBL6	,	53},
	{FM_REG_AGC_VAL_TBL7	,	59},
	{FM_REG_AGC_VAL_TBL8	,	71},
	{FM_REG_AGC_BOND_TBL0	,	-120UL},
	{FM_REG_AGC_BOND_TBL1	,	-89UL},
	{FM_REG_AGC_BOND_TBL2	,	-83UL},
	{FM_REG_AGC_BOND_TBL3	,	-77UL},
	{FM_REG_AGC_BOND_TBL4	,	-41UL},
	{FM_REG_AGC_BOND_TBL5	,	-35UL},
	{FM_REG_AGC_BOND_TBL6	,	-29UL},
	{FM_REG_AGC_BOND_TBL7	,	-21UL},
	{FM_REG_AGC_BOND_TBL8	,	-13UL},
	{FM_REG_AGC_BOND_TBL9	,	10},
	{FM_REG_SPUR_FREQ0               ,           0x0000081e}
};

#endif
