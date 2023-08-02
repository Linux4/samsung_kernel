#ifndef SR2351_FM_CTRL_H__
#define SR2351_FM_CTRL_H_

#include <mach/hardware.h>
#include <linux/sprd_2351.h>
#include <mach/sci_glb_regs.h>
#include <mach/sci.h>

#define	SR2351_FM_DEV_NAME	"Trout_FM"


extern int sr2351_fm_en(void);
extern int sr2351_fm_dis(void);
extern int sr2351_fm_get_status(int *status);
extern int sr2351_fm_get_frequency(u32 *);
extern void sr2351_fm_enter_sleep(void);
extern void sr2351_fm_exit_sleep(void);
extern void sr2351_fm_mute(void);
extern void sr2351_fm_unmute(void);
extern int sr2351_fm_init(void);
extern int sr2351_fm_deinit(void);
extern int sr2351_fm_set_tune(u32 freq);
extern int sr2351_fm_seek(u32 frequency, u32 seek_dir, u32 time_out, u32 *freq_found);
extern int sr2351_fm_stop_seek(void);
extern int sr2351_fm_get_rssi(u32 *);

/*FM Module Start*/
/* seek direction */
#define SR2351_SEEK_DIR_UP          0
#define SR2351_SEEK_DIR_DOWN        1
#define SR2351_SEEK_TIMEOUT         1

/** The following define the IOCTL command values via the ioctl macros */
#define FM_IOCTL_BASE     'R'
#define FM_IOCTL_ENABLE      _IOW(FM_IOCTL_BASE, 0, int)
#define FM_IOCTL_GET_ENABLE  _IOW(FM_IOCTL_BASE, 1, int)
#define FM_IOCTL_SET_TUNE    _IOW(FM_IOCTL_BASE, 2, int)
#define FM_IOCTL_GET_FREQ    _IOW(FM_IOCTL_BASE, 3, int)
#define FM_IOCTL_SEARCH      _IOW(FM_IOCTL_BASE, 4, int[4])
#define FM_IOCTL_STOP_SEARCH _IOW(FM_IOCTL_BASE, 5, int)
#define FM_IOCTL_MUTE        _IOW(FM_IOCTL_BASE, 6, int)
#define FM_IOCTL_SET_VOLUME  _IOW(FM_IOCTL_BASE, 7, int)
#define FM_IOCTL_GET_VOLUME  _IOW(FM_IOCTL_BASE, 8, int)
#define FM_IOCTL_CONFIG      _IOW(FM_IOCTL_BASE, 9, int)
#define FM_IOCTL_GET_RSSI    _IOW(FM_IOCTL_BASE, 10, int)


#ifdef CONFIG_FM_SEEK_STEP_50KHZ
#define MAX_FM_FREQ	10800
#define MIN_FM_FREQ	8750
#else
#define MAX_FM_FREQ	        1080
#define MIN_FM_FREQ	        875
#endif

#define FM_CTL_STI_MODE_NORMAL	0x0
#define	FM_CTL_STI_MODE_SEEK    0x1
#define	FM_CTL_STI_MODE_TUNE    0x2

#ifndef CONFIG_OF
#define SHARK_FM_REG_BASE               SPRD_FM_BASE
#define SHARK_APB_BASE_ADDR         	SPRD_AONAPB_BASE
#define  SHARK_PMU_BASE_ADDR       		SPRD_PMU_BASE
#define SHARK_AON_CLK_BASE_ADDR      	SPRD_AONCKG_BASE
#else
extern u32 rf2351_fm_get_fm_base(void);
extern u32 rf2351_fm_get_apb_base(void);
extern u32 rf2351_fm_get_pmu_base(void);
extern u32 rf2351_fm_get_aonckg_base(void);

#define SHARK_FM_REG_BASE			rf2351_fm_get_fm_base()
#define SHARK_APB_BASE_ADDR			rf2351_fm_get_apb_base()
#define SHARK_PMU_BASE_ADDR			rf2351_fm_get_pmu_base()
#define SHARK_AON_CLK_BASE_ADDR		rf2351_fm_get_aonckg_base()
#endif

#define	FM_REG_FM_CTRL                  (SHARK_FM_REG_BASE + 0x0000)
#define	FM_REG_FM_EN                    (SHARK_FM_REG_BASE + 0x0004)
#define	FM_REG_RF_INIT_GAIN             (SHARK_FM_REG_BASE + 0x0008)
#define	FM_REG_CHAN                     (SHARK_FM_REG_BASE + 0x000C)
#define	FM_REG_AGC_TBL_CLK              (SHARK_FM_REG_BASE + 0x0010)
#define	FM_REG_SEEK_LOOP                (SHARK_FM_REG_BASE + 0x0014)
#define	FM_REG_FMCTL_STI                (SHARK_FM_REG_BASE + 0x0018)
#define	FM_REG_BAND_LMT                 (SHARK_FM_REG_BASE + 0x001C)
#define	FM_REG_BAND_SPACE               (SHARK_FM_REG_BASE + 0x0020)
#define	FM_REG_RF_CTL                   (SHARK_FM_REG_BASE + 0x0024)
#define FM_REG_RF_CTL1                  (SHARK_FM_REG_BASE + 0x0028)
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
#define	FM_REG_HW_MUTE		 	(SHARK_FM_REG_BASE + 0x0084)
#define	FM_REG_SW_MUTE		 	(SHARK_FM_REG_BASE + 0x0088)
#define	FM_REG_UPD_CTRL		 	(SHARK_FM_REG_BASE + 0x008C)
#define	FM_REG_AUD_BLD0			(SHARK_FM_REG_BASE + 0x0090)
#define	FM_REG_AUD_BLD1		 	(SHARK_FM_REG_BASE + 0x0094)
#define	FM_REG_AGC_HYS		 	(SHARK_FM_REG_BASE + 0x0098)
#define	FM_REG_MONO_PWR		 	(SHARK_FM_REG_BASE + 0x009C)
#define	FM_REG_RF_CFG_DLY    		(SHARK_FM_REG_BASE + 0x00A0)
#define	FM_REG_AGC_TBL_STS	 	(SHARK_FM_REG_BASE + 0x00A4)
#define	FM_REG_ADP_BIT_SFT	 	(SHARK_FM_REG_BASE + 0x00A8)
#define	FM_REG_SEEK_CNT		 	(SHARK_FM_REG_BASE + 0x00AC)
#define	FM_REG_RSSI_STS		 	(SHARK_FM_REG_BASE + 0x00B0)
#define	FM_REG_CHAN_FREQ_STS	 	(SHARK_FM_REG_BASE + 0x00B8)
#define	FM_REG_FREQ_OFF_STS	     	(SHARK_FM_REG_BASE + 0x00BC)
#define	FM_REG_INPWR_STS	     	(SHARK_FM_REG_BASE + 0x00C0)
#define	FM_REG_RF_RSSI_STS	     	(SHARK_FM_REG_BASE + 0x00C4)
#define	FM_REG_NO_LPF_STS	     	(SHARK_FM_REG_BASE + 0x00C8)
#define	FM_REG_SMUTE_STS	     	(SHARK_FM_REG_BASE + 0x00CC)
#define	FM_REG_WBRSSI_STS	     	(SHARK_FM_REG_BASE + 0x00D0)
#define	FM_REG_AGC_BITS_TBL0	 	(SHARK_FM_REG_BASE + 0x0100)
#define	FM_REG_AGC_BITS_TBL1	 	(SHARK_FM_REG_BASE + 0x0104)
#define	FM_REG_AGC_BITS_TBL2	 	(SHARK_FM_REG_BASE + 0x0108)
#define	FM_REG_AGC_BITS_TBL3	 	(SHARK_FM_REG_BASE + 0x010C)
#define	FM_REG_AGC_BITS_TBL4	 	(SHARK_FM_REG_BASE + 0x0110)
#define	FM_REG_AGC_BITS_TBL5	 	(SHARK_FM_REG_BASE + 0x0114)
#define	FM_REG_AGC_BITS_TBL6	 	(SHARK_FM_REG_BASE + 0x0118)
#define	FM_REG_AGC_BITS_TBL7	 	(SHARK_FM_REG_BASE + 0x011C)
#define	FM_REG_AGC_BITS_TBL8	 	(SHARK_FM_REG_BASE + 0x0120)
#define	FM_REG_AGC_BITS_TBL9	 	(SHARK_FM_REG_BASE + 0x0124)
#define	FM_REG_AGC_BITS_TBL10	 	(SHARK_FM_REG_BASE + 0x0128)
#define	FM_REG_AGC_BITS_TBL11	 	(SHARK_FM_REG_BASE + 0x012C)
#define	FM_REG_AGC_BITS_TBL12	 	(SHARK_FM_REG_BASE + 0x0130)
#define	FM_REG_AGC_BITS_TBL13	 	(SHARK_FM_REG_BASE + 0x0134)
#define	FM_REG_AGC_BITS_TBL14	 	(SHARK_FM_REG_BASE + 0x0138)
#define	FM_REG_AGC_BITS_TBL15	 	(SHARK_FM_REG_BASE + 0x013C)
#define	FM_REG_AGC_BITS_TBL16	 	(SHARK_FM_REG_BASE + 0x0140)
#define	FM_REG_AGC_BITS_TBL17	 	(SHARK_FM_REG_BASE + 0x0144)
#define	FM_REG_AGC_VAL_TBL0		(SHARK_FM_REG_BASE + 0x0200)
#define	FM_REG_AGC_VAL_TBL1		(SHARK_FM_REG_BASE + 0x0204)
#define	FM_REG_AGC_VAL_TBL2		(SHARK_FM_REG_BASE + 0x0208)
#define	FM_REG_AGC_VAL_TBL3		(SHARK_FM_REG_BASE + 0x020C)
#define	FM_REG_AGC_VAL_TBL4		(SHARK_FM_REG_BASE + 0x0210)
#define	FM_REG_AGC_VAL_TBL5		(SHARK_FM_REG_BASE + 0x0214)
#define	FM_REG_AGC_VAL_TBL6		(SHARK_FM_REG_BASE + 0x0218)
#define	FM_REG_AGC_VAL_TBL7		(SHARK_FM_REG_BASE + 0x021C)
#define	FM_REG_AGC_VAL_TBL8		(SHARK_FM_REG_BASE + 0x0220)
#define	FM_REG_AGC_VAL_TBL9		(SHARK_FM_REG_BASE + 0x0224)
#define	FM_REG_AGC_VAL_TBL10		(SHARK_FM_REG_BASE + 0x0228)
#define	FM_REG_AGC_VAL_TBL11	 	(SHARK_FM_REG_BASE + 0x022C)
#define	FM_REG_AGC_VAL_TBL12	 	(SHARK_FM_REG_BASE + 0x0230)
#define	FM_REG_AGC_VAL_TBL13	 	(SHARK_FM_REG_BASE + 0x0234)
#define	FM_REG_AGC_VAL_TBL14	 	(SHARK_FM_REG_BASE + 0x0238)
#define	FM_REG_AGC_VAL_TBL15	 	(SHARK_FM_REG_BASE + 0x023C)
#define	FM_REG_AGC_VAL_TBL16	 	(SHARK_FM_REG_BASE + 0x0240)
#define	FM_REG_AGC_VAL_TBL17	 	(SHARK_FM_REG_BASE + 0x0244)
#define	FM_REG_AGC_BOND_TBL0	 	(SHARK_FM_REG_BASE + 0x0300)
#define	FM_REG_AGC_BOND_TBL1	 	(SHARK_FM_REG_BASE + 0x0304)
#define	FM_REG_AGC_BOND_TBL2	 	(SHARK_FM_REG_BASE + 0x0308)
#define	FM_REG_AGC_BOND_TBL3	 	(SHARK_FM_REG_BASE + 0x030C)
#define	FM_REG_AGC_BOND_TBL4	 	(SHARK_FM_REG_BASE + 0x0310)
#define	FM_REG_AGC_BOND_TBL5	 	(SHARK_FM_REG_BASE + 0x0314)
#define	FM_REG_AGC_BOND_TBL6	 	(SHARK_FM_REG_BASE + 0x0318)
#define	FM_REG_AGC_BOND_TBL7	 	(SHARK_FM_REG_BASE + 0x031C)
#define	FM_REG_AGC_BOND_TBL8	 	(SHARK_FM_REG_BASE + 0x0320)
#define	FM_REG_AGC_BOND_TBL9	 	(SHARK_FM_REG_BASE + 0x0324)

#define FM_REG_SPUR_FREQ0       	(SHARK_FM_REG_BASE + 0x340) /* new reg*/
#define FM_REG_SPUR_FREQ1       	(SHARK_FM_REG_BASE + 0X344) /* new reg*/

#define FM_REG_SPUR_RM_CTL       		(SHARK_FM_REG_BASE + 0X348)
#define FM_SPUR_RM_CTL_DEFAULT_VALUE	(0X88FFFF)
#define FM_SPUR_RM_CTL_SET_VALUE		(0X551F1F)
#define FM_REG_SPUR_DC_RM_CTL       	(SHARK_FM_REG_BASE + 0X34C)
#define FM_SPUR_DC_RM_CTL_DEFAULT_VALUE	(0X10FFFF)
#define FM_SPUR_DC_RM_CTL_SET_VALUE		(0XA03FF)


#define	FM_REG_AGC_DB_TBL_BEGIN	 	(SHARK_FM_REG_BASE + 0x0500)
#define FM_REG_AGC_DB_TBL_END	 	(SHARK_FM_REG_BASE + 0x05A0)
#define FM_REG_AGC_DB_TBL_CNT	 \
	((0x5a0-0x500)/4 + 1)

#define	FM_REG_SPI_CTL			(SHARK_FM_REG_BASE + 0x0810)
#define	FM_REG_SPI_WD0			(SHARK_FM_REG_BASE + 0x0814)
#define FM_REG_SPI_WD1			(SHARK_FM_REG_BASE + 0x0818)
#define FM_REG_SPI_RD			(SHARK_FM_REG_BASE + 0x081C)
#define FM_REG_SPI_FIFO_STS		(SHARK_FM_REG_BASE + 0x0820)

#define SHARK_APB_EB0                  REG_AON_APB_APB_EB0 
#define SHARK_APB_EB1                  REG_AON_APB_APB_EB1
#define SHARK_APB_RST0                 REG_AON_APB_APB_RST0
#define SHARK_APB_EB0_SET              (REG_AON_APB_APB_EB0+0x1000)

#define  SHARK_PMU_SLEEP_CTRL           REG_PMU_APB_SLEEP_CTRL

#define SHARK_PMU_APB_MEM_PD_CFG0		REG_PMU_APB_MEM_PD_CFG0

#if defined(CONFIG_ARCH_SCX30G)
#define SHARK_MSPI_CLK_SWITCH       (SHARK_AON_CLK_BASE_ADDR + 0x0054)
#elif defined(CONFIG_ARCH_SCX15) || defined(CONFIG_ARCH_SCX35)
#define SHARK_MSPI_CLK_SWITCH       (SHARK_AON_CLK_BASE_ADDR + 0x0050)
#endif

#define  SHARK_PMU_APB_PD_AP_SYS_CFG    REG_PMU_APB_PD_AP_SYS_CFG

#define INT_NUM_FM_test 27

#define SHARK_CHIP_BD   1

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

/*FM Module End*/

/*2351 RF Start*/

#define	FM_SR2351_ADC_CLK            0x06F  
#define	FM_SR2351_DCOC_CAL_TIMER     0x471
#define	FM_SR2351_RC_TUNER           0x477
#define	FM_SR2351_RX_ADC_CLK         0x478
#define	FM_SR2351_OVERLOAD_DET       0x431
#define	FM_SR2351_RX_GAIN            0x404
#define	FM_SR2351_MODE               0x400
#define	FM_SR2351_FREQ               0x402

/*2351 RF End*/

#define SR2351_PRINT(format, arg...)  \
	do { \
		printk("sr2351_fm %s-%d -- "format"\n", \
		__func__, __LINE__, ## arg); \
	} while (0)
	
#endif
