#include "../pwrcal-env.h"
#include "../pwrcal-rae.h"
#include "S5E7880-sfrbase.h"
#include "S5E7880-vclk-internal.h"
#include "S5E7880-pmusfr.h"


#ifdef PWRCAL_TARGET_LINUX
#include <soc/samsung/ect_parser.h>
#else
#include <mach/ect_parser.h>
#endif


#ifndef MHZ
#define MHZ		((unsigned long long)1000000)
#endif

#define DMC_WAIT_CNT 10000

#define SAVED_LP4_DIE_DENSITY_BASE		((void *)(0x0215D000 + 0x4C00 + 0x4))		/* Alive SRAM : Saved LP4 die density location */

#define DREX0_MemControl		((void *)(DREX0_BASE + 0x0004))
#define DREX0_CgControl			((void *)(DREX0_BASE + 0x0008))
#define DREX0_TimingRfcPb		((void *)(DREX0_BASE + 0x0020))
#define DREX0_PwrDnConfig		((void *)(DREX0_BASE + 0x0028))
#define DREX0_TickGranularity	((void *)(DREX0_BASE + 0x002C))
#define DREX0_TimingRow_0		((void *)(DREX0_BASE + 0x0034))
#define DREX0_TimingData_0		((void *)(DREX0_BASE + 0x0038))
#define DREX0_TimingPower_0		((void *)(DREX0_BASE + 0x003C))
#define DREX0_RdFetch_0			((void *)(DREX0_BASE + 0x004C))
#define DREX0_EtcControl_0		((void *)(DREX0_BASE + 0x0058))
#define DREX0_Train_Timing_0		((void *)(DREX0_BASE + 0x0410))
#define DREX0_Hw_Ptrain_Period_0	((void *)(DREX0_BASE + 0x0420))
#define DREX0_Hwpr_Train_Config_0	((void *)(DREX0_BASE + 0x0440))
#define DREX0_Hwpr_Train_Control_0	((void *)(DREX0_BASE + 0x0450))
#define DREX0_TimingRow_1		((void *)(DREX0_BASE + 0x00E4))
#define DREX0_TimingData_1		((void *)(DREX0_BASE + 0x00E8))
#define DREX0_TimingPower_1		((void *)(DREX0_BASE + 0x00EC))
#define DREX0_RdFetch_1			((void *)(DREX0_BASE + 0x0050))
#define DREX0_EtcControl_1		((void *)(DREX0_BASE + 0x005C))
#define DREX0_Train_Timing_1		((void *)(DREX0_BASE + 0x0414))
#define DREX0_Hw_Ptrain_Period_1	((void *)(DREX0_BASE + 0x0424))
#define DREX0_Hwpr_Train_Config_1	((void *)(DREX0_BASE + 0x0444))
#define DREX0_Hwpr_Train_Control_1	((void *)(DREX0_BASE + 0x0454))
#define DREX0_Pchannel_Pause_State	((void *)(DREX0_BASE + 0x0500))
#define DREX0_PhyStatus			((void *)(DREX0_BASE + 0x0040))

#define DREX1_MemControl		((void *)(DREX1_BASE + 0x0004))
#define DREX1_CgControl			((void *)(DREX1_BASE + 0x0008))
#define DREX1_TimingRfcPb		((void *)(DREX1_BASE + 0x0020))
#define DREX1_PwrDnConfig		((void *)(DREX1_BASE + 0x0028))
#define DREX1_TickGranularity	((void *)(DREX1_BASE + 0x002C))
#define DREX1_TimingRow_0		((void *)(DREX1_BASE + 0x0034))
#define DREX1_TimingData_0		((void *)(DREX1_BASE + 0x0038))
#define DREX1_TimingPower_0		((void *)(DREX1_BASE + 0x003C))
#define DREX1_RdFetch_0			((void *)(DREX1_BASE + 0x004C))
#define DREX1_EtcControl_0		((void *)(DREX1_BASE + 0x0058))
#define DREX1_Train_Timing_0		((void *)(DREX1_BASE + 0x0410))
#define DREX1_Hw_Ptrain_Period_0	((void *)(DREX1_BASE + 0x0420))
#define DREX1_Hwpr_Train_Config_0	((void *)(DREX1_BASE + 0x0440))
#define DREX1_Hwpr_Train_Control_0	((void *)(DREX1_BASE + 0x0450))
#define DREX1_TimingRow_1		((void *)(DREX1_BASE + 0x00E4))
#define DREX1_TimingData_1		((void *)(DREX1_BASE + 0x00E8))
#define DREX1_TimingPower_1		((void *)(DREX1_BASE + 0x00EC))
#define DREX1_RdFetch_1			((void *)(DREX1_BASE + 0x0050))
#define DREX1_EtcControl_1		((void *)(DREX1_BASE + 0x005C))
#define DREX1_Train_Timing_1		((void *)(DREX1_BASE + 0x0414))
#define DREX1_Hw_Ptrain_Period_1	((void *)(DREX1_BASE + 0x0424))
#define DREX1_Hwpr_Train_Config_1	((void *)(DREX1_BASE + 0x0444))
#define DREX1_Hwpr_Train_Control_1	((void *)(DREX1_BASE + 0x0454))
#define DREX1_Pchannel_Pause_State	((void *)(DREX1_BASE + 0x0500))
#define DREX1_PhyStatus			((void *)(DREX1_BASE + 0x040))

#define DREX_TimingRfcPb_Set1_MASK	0x0000FF00
#define DREX_TimingRfcPb_Set0_MASK	0x000000FF
#define DREX_TickGranularity_Set0_MASK	0x0000FFFF
#define DREX_TickGranularity_Set1_MASK	0xFFFF0000

#define PHY0_CAL_CON0		((void *)(DDRPHY0_BASE + 0x0004))
#define PHY0_LP_CON0		((void *)(DDRPHY0_BASE + 0x0018))
#define PHY0_DVFS_CON		((void *)(DDRPHY0_BASE + 0x00B8))
#define PHY0_DVFS0_CON0		((void *)(DDRPHY0_BASE + 0x00BC))
#define PHY0_DVFS1_CON0		((void *)(DDRPHY0_BASE + 0x00C0))
#define PHY0_DVFS0_CON1		((void *)(DDRPHY0_BASE + 0x00C4))
#define PHY0_DVFS1_CON1		((void *)(DDRPHY0_BASE + 0x00C8))
#define PHY0_DVFS0_CON2		((void *)(DDRPHY0_BASE + 0x00CC))
#define PHY0_DVFS1_CON2		((void *)(DDRPHY0_BASE + 0x00D0))
#define PHY0_DVFS0_CON3		((void *)(DDRPHY0_BASE + 0x00D4))
#define PHY0_DVFS1_CON3		((void *)(DDRPHY0_BASE + 0x00D8))
#define PHY0_DVFS0_CON4		((void *)(DDRPHY0_BASE + 0x00DC))
#define PHY0_DVFS1_CON4		((void *)(DDRPHY0_BASE + 0x00E0))
#define PHY0_ZQ_CON9	((void *)(DDRPHY0_BASE + 0x03EC))

#define PHY1_CAL_CON0		((void *)(DDRPHY1_BASE + 0x0004))
#define PHY1_LP_CON0		((void *)(DDRPHY1_BASE + 0x0018))
#define PHY1_DVFS_CON		((void *)(DDRPHY1_BASE + 0x00B8))
#define PHY1_DVFS0_CON0		((void *)(DDRPHY1_BASE + 0x00BC))
#define PHY1_DVFS1_CON0		((void *)(DDRPHY1_BASE + 0x00C0))
#define PHY1_DVFS0_CON1		((void *)(DDRPHY1_BASE + 0x00C4))
#define PHY1_DVFS1_CON1		((void *)(DDRPHY1_BASE + 0x00C8))
#define PHY1_DVFS0_CON2		((void *)(DDRPHY1_BASE + 0x00CC))
#define PHY1_DVFS1_CON2		((void *)(DDRPHY1_BASE + 0x00D0))
#define PHY1_DVFS0_CON3		((void *)(DDRPHY1_BASE + 0x00D4))
#define PHY1_DVFS1_CON3		((void *)(DDRPHY1_BASE + 0x00D8))
#define PHY1_DVFS0_CON4		((void *)(DDRPHY1_BASE + 0x00DC))
#define PHY1_DVFS1_CON4		((void *)(DDRPHY1_BASE + 0x00E0))
#define PHY1_ZQ_CON9	((void *)(DDRPHY1_BASE + 0x03EC))

#define PHY_DVFSCON2_CMOS_DIFF_MASK		(0xFF000000)
#define PHY_DVFSCON2_USE_SE				(0x0<<24)

/* PHY DVFS CON SFR BIT DEFINITION */

#define DREX0_DIRECTCMD		((void *)(DREX0_BASE + 0x0010))
#define DREX1_DIRECTCMD		((void *)(DREX1_BASE + 0x0010))

#define VTMON0_VT_MON_CON		((void *)(VTMON0_BASE + 0x0014))
#define VTMON0_DREX_TIMING_SW_SET	((void *)(VTMON0_BASE + 0x003C))
#define VTMON0_PER_UPDATE_STATUS	((void *)(VTMON0_BASE + 0x0064))
#define VTMON0_MIF_DREX_WDATA0	((void *)(VTMON0_BASE + 0x001C))
#define VTMON1_VT_MON_CON		((void *)(VTMON1_BASE + 0x0014))
#define VTMON1_DREX_TIMING_SW_SET	((void *)(VTMON1_BASE + 0x003C))
#define VTMON1_PER_UPDATE_STATUS	((void *)(VTMON1_BASE + 0x0064))
#define VTMON1_MIF_DREX_WDATA0	((void *)(VTMON1_BASE + 0x001C))

#define VTMON0_CNT_LIMIT_SET0_SW0	((void *)(VTMON0_BASE + 0x0080))
#define VTMON0_CNT_LIMIT_SET1_SW0	((void *)(VTMON0_BASE + 0x0084))
#define VTMON0_CNT_LIMIT_SET0_SW1	((void *)(VTMON0_BASE + 0x0088))
#define VTMON0_CNT_LIMIT_SET1_SW1	((void *)(VTMON0_BASE + 0x008C))
#define VTMON1_CNT_LIMIT_SET0_SW0	((void *)(VTMON1_BASE + 0x0080))
#define VTMON1_CNT_LIMIT_SET1_SW0	((void *)(VTMON1_BASE + 0x0084))
#define VTMON1_CNT_LIMIT_SET0_SW1	((void *)(VTMON1_BASE + 0x0088))
#define VTMON1_CNT_LIMIT_SET1_SW1	((void *)(VTMON1_BASE + 0x008C))



/* Write (tDQS2DQ) compensation */

#define PERIODIC_WR_TRAIN 1


#if PERIODIC_WR_TRAIN
#define dvfs_offset 16
#else
#define dvfs_offset 0
#endif


enum dmc_dvfs_mif_level_idx {
	DMC_DVFS_MIF_L0,
	DMC_DVFS_MIF_L1,
	DMC_DVFS_MIF_L2,
	DMC_DVFS_MIF_L3,
	DMC_DVFS_MIF_L4,
	DMC_DVFS_MIF_L5,
	DMC_DVFS_MIF_L6,
	DMC_DVFS_MIF_L7,
	DMC_DVFS_MIF_L8,
	DMC_DVFS_MIF_L9,
	DMC_DVFS_MIF_L10,
	DMC_DVFS_MIF_L11,
	DMC_DVFS_MIF_L12,
	DMC_DVFS_MIF_L13,
	DMC_DVFS_MIF_L14,
	COUNT_OF_CMU_DVFS_MIF_LEVEL,
	CMU_DVFS_MIF_INVALID = 0xFF,
};

enum dmc_timing_set_idx {
	DMC_TIMING_SET_0,
	DMC_TIMING_SET_1
};

#if 1
typedef enum {
	LP4_4Gb_Die = 0,
	LP4_6Gb_Die = 1,
	LP4_8Gb_Die = 2,
	LP4_12Gb_Die = 3,
	LP4_16Gb_Die = 4,
	LP4_24Gb_Die = 5,
	LP4_32Gb_Die = 6,
	LP4_Invalid_Die = 7,
} lp4_density_t;

typedef enum {
	LP4_SEC = 1,
	LP4_NONSEC = 2,
} lp4_vendor_t;
#endif


typedef struct {
	unsigned int drex_TimingRfcPb;
	unsigned int drex_TimingRow;
	unsigned int drex_TimingData;
	unsigned int drex_TimingPower;
	unsigned int drex_RdFetch;
	unsigned int drex_EtcControl;
	unsigned int drex_Train_Timing;
	unsigned int drex_Hw_Ptrain_Period;
	unsigned int drex_Hwpr_Train_Config;
	unsigned int drex_Hwpr_Train_Control;
	unsigned int drex_TickGranularity;
	unsigned int directCmdMr13;
	unsigned int directCmdMr1;
	unsigned int directCmdMr2;
	unsigned int directCmdMr3;
	unsigned int directCmdMr22;
	unsigned int directCmdMr11;
	unsigned int directCmdMr14;
	unsigned int directCmdMr12;
	unsigned int timingSetSw;
} dmc_drex_dfs_mif_table;

typedef struct  {
	unsigned int phy_Dvfs_Con0;
	unsigned int phy_Dvfs_Con1;
	unsigned int phy_Dvfs_Con2;
	unsigned int phy_Dvfs_Con3;
	unsigned int phy_Dvfs_Con4;
} dmc_phy_dfs_mif_table;

typedef struct  {
	unsigned int vtmon_Cnt_Limit_set0;
	unsigned int vtmon_Cnt_Limit_set1;
} dmc_vtmon_dfs_mif_table;

#define LP4_RL 12 /* 16, 12 */
#define LP4_WL 6 /* 8, 6 */
#define LP4_RL_L10 6 /* 16, 12 */
#define LP4_WL_L10 4 /* 8, 6 */
#define DRAM_RLWL 0x09
#define DRAM_RLWL_L10 0x00

#if 0
static const unsigned long long mif_freq_to_level[] = {
	1794 * MHZ,
	1716 * MHZ,
	1539 * MHZ,
	1352 * MHZ,
	1144 * MHZ,
	1014 * MHZ,
	845 * MHZ,
	676 * MHZ,
	546 * MHZ,
	421 * MHZ,
	286 * MHZ,
	208 * MHZ,
//	133 * MHZ,
//	100 * MHZ,
//	50 * MHZ,
};
#endif

//static int is_dvfs_cold_start = 1;

static dmc_drex_dfs_mif_table *pwrcal_gadfs_drex_mif_table;
static dmc_phy_dfs_mif_table *pwrcal_gadfs_phy_mif_table;
static dmc_vtmon_dfs_mif_table *pwrcal_gadfs_vtmon_mif_table;

static unsigned long long *mif_freq_to_level;
static int num_mif_freq_to_level;


static unsigned int convert_to_level(unsigned long long freq)
{
	int idx;
	int tablesize = num_mif_freq_to_level;

	for (idx = tablesize - 1; idx >= 0; idx--)
		if (freq <= mif_freq_to_level[idx])
			return (unsigned int)idx;

	return 0;
}

dmc_drex_dfs_mif_table *cur_drex_param;
dmc_phy_dfs_mif_table *cur_phy_param;

void pwrcal_dmc_set_dvfs(unsigned long long target_mif_freq, unsigned int timing_set_idx)
{
	unsigned int uReg;
	unsigned int target_mif_level_idx;
	unsigned int initial_mif_freq;

	target_mif_level_idx = convert_to_level(target_mif_freq);

	cur_drex_param = pwrcal_gadfs_drex_mif_table;
	cur_phy_param = pwrcal_gadfs_phy_mif_table;

	initial_mif_freq = pwrcal_readl(PHY0_DVFS_CON)&0x7ff;
	initial_mif_freq *= 1000000;

	/* 1. Configure parameter */
	if (timing_set_idx == DMC_TIMING_SET_0) {
		/* DREX0 Settings */
		uReg = pwrcal_readl(DREX0_TimingRfcPb);
		uReg &= ~(DREX_TimingRfcPb_Set0_MASK);
		uReg |= (cur_drex_param[target_mif_level_idx].drex_TimingRfcPb & (DREX_TimingRfcPb_Set0_MASK));
		pwrcal_writel(DREX0_TimingRfcPb, uReg);
		pwrcal_writel(DREX0_TimingRow_0, cur_drex_param[target_mif_level_idx].drex_TimingRow);
		pwrcal_writel(DREX0_TimingData_0, cur_drex_param[target_mif_level_idx].drex_TimingData);
		pwrcal_writel(DREX0_TimingPower_0, cur_drex_param[target_mif_level_idx].drex_TimingPower);
		pwrcal_writel(DREX0_RdFetch_0, cur_drex_param[target_mif_level_idx].drex_RdFetch);
		pwrcal_writel(DREX0_EtcControl_0, (DMC_TIMING_SET_0 << 7) | cur_drex_param[target_mif_level_idx].drex_EtcControl);
		pwrcal_writel(DREX0_Train_Timing_0, cur_drex_param[target_mif_level_idx].drex_Train_Timing);
		pwrcal_writel(DREX0_Hw_Ptrain_Period_0, cur_drex_param[target_mif_level_idx].drex_Hw_Ptrain_Period);
		pwrcal_writel(DREX0_Hwpr_Train_Config_0, cur_drex_param[target_mif_level_idx].drex_Hwpr_Train_Config);
		pwrcal_writel(DREX0_Hwpr_Train_Control_0, cur_drex_param[target_mif_level_idx].drex_Hwpr_Train_Control);

		uReg = pwrcal_readl(DREX0_TickGranularity);
		uReg &= ~(DREX_TickGranularity_Set0_MASK);
		uReg |= (cur_drex_param[target_mif_level_idx].drex_TickGranularity & (DREX_TickGranularity_Set0_MASK));
		pwrcal_writel(DREX0_TickGranularity, uReg);  //added 15/11/03

		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con0);
		pwrcal_writel(PHY0_DVFS0_CON0, uReg);
		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con1);
		pwrcal_writel(PHY0_DVFS0_CON1, uReg);
		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con2);
		pwrcal_writel(PHY0_DVFS0_CON2, uReg);
		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con3);
		pwrcal_writel(PHY0_DVFS0_CON3, uReg);
		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con4);
		pwrcal_writel(PHY0_DVFS0_CON4, uReg);

		uReg = pwrcal_readl(PHY0_DVFS_CON);
		uReg &= ~(0x3<<30);
		if (target_mif_level_idx == initial_mif_freq)
			uReg |= 0x0<<30;
		else
			uReg |= 0x1<<30;

		pwrcal_writel(PHY0_DVFS_CON, uReg);

		pwrcal_writel(VTMON0_CNT_LIMIT_SET0_SW0, pwrcal_gadfs_vtmon_mif_table[target_mif_level_idx].vtmon_Cnt_Limit_set0);
		pwrcal_writel(VTMON0_CNT_LIMIT_SET1_SW0, pwrcal_gadfs_vtmon_mif_table[target_mif_level_idx].vtmon_Cnt_Limit_set1);

		//FSP OP=1 FSP WR=0
		pwrcal_writel(DREX0_DIRECTCMD, (0x90 << 2) | (cur_drex_param[target_mif_level_idx].directCmdMr13));
		pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr1);
		pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr2);
		pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr3);
		pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr12);
		pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr14);
		pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr22);
		pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr11);
		/*		pwrcal_writel( DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].drex_DirectCmd_mr14);	*/

		pwrcal_writel(VTMON0_DREX_TIMING_SW_SET, (0x0 << 0) | (cur_drex_param[target_mif_level_idx].timingSetSw));
		pwrcal_writel(VTMON0_MIF_DREX_WDATA0, (0x10 << 2) | (cur_drex_param[target_mif_level_idx].directCmdMr13));


		/* DREX1 Settings */
		uReg = pwrcal_readl(DREX1_TimingRfcPb);
		uReg &= ~(DREX_TimingRfcPb_Set0_MASK);
		uReg |= (cur_drex_param[target_mif_level_idx].drex_TimingRfcPb & (DREX_TimingRfcPb_Set0_MASK));
		pwrcal_writel(DREX1_TimingRfcPb, uReg);
		pwrcal_writel(DREX1_TimingRow_0, cur_drex_param[target_mif_level_idx].drex_TimingRow);
		pwrcal_writel(DREX1_TimingData_0, cur_drex_param[target_mif_level_idx].drex_TimingData);
		pwrcal_writel(DREX1_TimingPower_0, cur_drex_param[target_mif_level_idx].drex_TimingPower);
		pwrcal_writel(DREX1_RdFetch_0, cur_drex_param[target_mif_level_idx].drex_RdFetch);
		pwrcal_writel(DREX1_EtcControl_0, (DMC_TIMING_SET_0 << 7) | cur_drex_param[target_mif_level_idx].drex_EtcControl);
		pwrcal_writel(DREX1_Train_Timing_0, cur_drex_param[target_mif_level_idx].drex_Train_Timing);
		pwrcal_writel(DREX1_Hw_Ptrain_Period_0, cur_drex_param[target_mif_level_idx].drex_Hw_Ptrain_Period);
		pwrcal_writel(DREX1_Hwpr_Train_Config_0, cur_drex_param[target_mif_level_idx].drex_Hwpr_Train_Config);
		pwrcal_writel(DREX1_Hwpr_Train_Control_0, cur_drex_param[target_mif_level_idx].drex_Hwpr_Train_Control);

		uReg = pwrcal_readl(DREX1_TickGranularity);
		uReg &= ~(DREX_TickGranularity_Set0_MASK);
		uReg |= (cur_drex_param[target_mif_level_idx].drex_TickGranularity & (DREX_TickGranularity_Set0_MASK));
		pwrcal_writel(DREX1_TickGranularity, uReg);  //added 15/11/03

		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con0);
		pwrcal_writel(PHY1_DVFS0_CON0, uReg);
		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con1);
		pwrcal_writel(PHY1_DVFS0_CON1, uReg);
		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con2);
		pwrcal_writel(PHY1_DVFS0_CON2, uReg);
		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con3);
		pwrcal_writel(PHY1_DVFS0_CON3, uReg);
		uReg = *((unsigned int *)&cur_phy_param[target_mif_level_idx].phy_Dvfs_Con4);
		pwrcal_writel(PHY1_DVFS0_CON4, uReg);

		uReg = pwrcal_readl(PHY1_DVFS_CON);
		uReg &= ~(0x3<<30);
		if (target_mif_level_idx <= 2)
			uReg |= 0x0<<30;
		else
			uReg |= 0x1<<30;

		pwrcal_writel(PHY1_DVFS_CON, uReg);

		pwrcal_writel(VTMON1_CNT_LIMIT_SET0_SW0, pwrcal_gadfs_vtmon_mif_table[target_mif_level_idx].vtmon_Cnt_Limit_set0);
		pwrcal_writel(VTMON1_CNT_LIMIT_SET1_SW0, pwrcal_gadfs_vtmon_mif_table[target_mif_level_idx].vtmon_Cnt_Limit_set1);

		//FSP OP=1 FSP WR=0
		pwrcal_writel(DREX1_DIRECTCMD, (0x90 << 2) | (cur_drex_param[target_mif_level_idx].directCmdMr13));
		pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr1);
		pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr2);
		pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr3);
		pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr12);
		pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr14);
		pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr22);
		pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr11);
		/*		pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].drex_DirectCmd_mr14);	*/

		pwrcal_writel(VTMON1_DREX_TIMING_SW_SET, (0x0 << 0) | (cur_drex_param[target_mif_level_idx].timingSetSw));
		pwrcal_writel(VTMON1_MIF_DREX_WDATA0, (0x10 << 2) | (cur_drex_param[target_mif_level_idx].directCmdMr13));

	} else if (timing_set_idx == DMC_TIMING_SET_1) {
		/* DREX0 Settings */
			uReg = pwrcal_readl(DREX0_TimingRfcPb);
			uReg &= ~(DREX_TimingRfcPb_Set1_MASK);
			uReg |= (cur_drex_param[target_mif_level_idx].drex_TimingRfcPb & (DREX_TimingRfcPb_Set1_MASK));
			pwrcal_writel(DREX0_TimingRfcPb, uReg);
			pwrcal_writel(DREX0_TimingRow_1, cur_drex_param[target_mif_level_idx].drex_TimingRow);
			pwrcal_writel(DREX0_TimingData_1, cur_drex_param[target_mif_level_idx].drex_TimingData);
			pwrcal_writel(DREX0_TimingPower_1, cur_drex_param[target_mif_level_idx].drex_TimingPower);
			pwrcal_writel(DREX0_RdFetch_1, cur_drex_param[target_mif_level_idx].drex_RdFetch);
			pwrcal_writel(DREX0_EtcControl_1, (DMC_TIMING_SET_1 << 7) | cur_drex_param[target_mif_level_idx].drex_EtcControl);
			pwrcal_writel(DREX0_Train_Timing_1, cur_drex_param[target_mif_level_idx].drex_Train_Timing);
			pwrcal_writel(DREX0_Hw_Ptrain_Period_1, cur_drex_param[target_mif_level_idx].drex_Hw_Ptrain_Period);
			pwrcal_writel(DREX0_Hwpr_Train_Config_1, cur_drex_param[target_mif_level_idx].drex_Hwpr_Train_Config);
			pwrcal_writel(DREX0_Hwpr_Train_Control_1, cur_drex_param[target_mif_level_idx].drex_Hwpr_Train_Control);

			uReg = pwrcal_readl(DREX0_TickGranularity);
			uReg &= ~(DREX_TickGranularity_Set1_MASK);
			uReg |= (cur_drex_param[target_mif_level_idx].drex_TickGranularity & (DREX_TickGranularity_Set1_MASK));
			pwrcal_writel(DREX0_TickGranularity, uReg);  //added 15/11/03

			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con0;
			pwrcal_writel(PHY0_DVFS1_CON0, uReg);
			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con1;
			pwrcal_writel(PHY0_DVFS1_CON1, uReg);
			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con2;
			uReg &= ~(PHY_DVFSCON2_CMOS_DIFF_MASK);
			uReg |= PHY_DVFSCON2_USE_SE;  //added 15/11/10
			pwrcal_writel(PHY0_DVFS1_CON2, uReg);
			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con3;
			pwrcal_writel(PHY0_DVFS1_CON3, uReg);
			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con4;
			pwrcal_writel(PHY0_DVFS1_CON4, uReg);

			uReg = pwrcal_readl(PHY0_DVFS_CON);
			uReg &= ~(0x3<<30);
			uReg |= 0x2<<30;
			pwrcal_writel(PHY0_DVFS_CON, uReg);
//			pwrcal_writel(PHY0_DVFS_CON, 0x2<<30 | 1539);

			pwrcal_writel(VTMON0_CNT_LIMIT_SET0_SW0, pwrcal_gadfs_vtmon_mif_table[target_mif_level_idx].vtmon_Cnt_Limit_set0);
			pwrcal_writel(VTMON0_CNT_LIMIT_SET1_SW0, pwrcal_gadfs_vtmon_mif_table[target_mif_level_idx].vtmon_Cnt_Limit_set1);

			//FSP OP=0, FSP-WR=1
			pwrcal_writel(DREX0_DIRECTCMD, (0x50 << 2) | (cur_drex_param[target_mif_level_idx].directCmdMr13));
			pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr1);
			pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr2);
			pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr3);
			pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr12);
			pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr14);
			pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr22);
			pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr11);
			/*		pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].drex_DirectCmd_mr14);	*/

			pwrcal_writel(VTMON0_DREX_TIMING_SW_SET, (0x1 << 0) | (cur_drex_param[target_mif_level_idx].timingSetSw));
			pwrcal_writel(VTMON0_MIF_DREX_WDATA0, (0xD0 << 2) | (cur_drex_param[target_mif_level_idx].directCmdMr13));


		/* DREX1 Settings */
			uReg = pwrcal_readl(DREX1_TimingRfcPb);
			uReg &= ~(DREX_TimingRfcPb_Set1_MASK);
			uReg |= (cur_drex_param[target_mif_level_idx].drex_TimingRfcPb & (DREX_TimingRfcPb_Set1_MASK));
			pwrcal_writel(DREX1_TimingRfcPb, uReg);
			pwrcal_writel(DREX1_TimingRow_1, cur_drex_param[target_mif_level_idx].drex_TimingRow);
			pwrcal_writel(DREX1_TimingData_1, cur_drex_param[target_mif_level_idx].drex_TimingData);
			pwrcal_writel(DREX1_TimingPower_1, cur_drex_param[target_mif_level_idx].drex_TimingPower);
			pwrcal_writel(DREX1_RdFetch_1, cur_drex_param[target_mif_level_idx].drex_RdFetch);
			pwrcal_writel(DREX1_EtcControl_1, (DMC_TIMING_SET_1 << 7) | cur_drex_param[target_mif_level_idx].drex_EtcControl);
			pwrcal_writel(DREX1_Train_Timing_1, cur_drex_param[target_mif_level_idx].drex_Train_Timing);
			pwrcal_writel(DREX1_Hw_Ptrain_Period_1, cur_drex_param[target_mif_level_idx].drex_Hw_Ptrain_Period);
			pwrcal_writel(DREX1_Hwpr_Train_Config_1, cur_drex_param[target_mif_level_idx].drex_Hwpr_Train_Config);
			pwrcal_writel(DREX1_Hwpr_Train_Control_1, cur_drex_param[target_mif_level_idx].drex_Hwpr_Train_Control);

			uReg = pwrcal_readl(DREX1_TickGranularity);
			uReg &= ~(DREX_TickGranularity_Set1_MASK);
			uReg |= (cur_drex_param[target_mif_level_idx].drex_TickGranularity & (DREX_TickGranularity_Set1_MASK));
			pwrcal_writel(DREX1_TickGranularity, uReg);  //added 15/11/03

			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con0;
			pwrcal_writel(PHY1_DVFS1_CON0, uReg);
			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con1;
			pwrcal_writel(PHY1_DVFS1_CON1, uReg);
			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con2;
			uReg &= ~(PHY_DVFSCON2_CMOS_DIFF_MASK);
			uReg |= PHY_DVFSCON2_USE_SE;  //added 15/11/10
			pwrcal_writel(PHY1_DVFS1_CON2, uReg);
			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con3;
			pwrcal_writel(PHY1_DVFS1_CON3, uReg);
			uReg = cur_phy_param[target_mif_level_idx].phy_Dvfs_Con4;
			pwrcal_writel(PHY1_DVFS1_CON4, uReg);

			uReg = pwrcal_readl(PHY1_DVFS_CON);
			uReg &= ~(0x3<<30);
			uReg |= 0x2<<30;
			pwrcal_writel(PHY1_DVFS_CON, uReg);
//			pwrcal_writel(PHY0_DVFS_CON, 0x2<<30 | 1539);

			pwrcal_writel(VTMON1_CNT_LIMIT_SET0_SW0, pwrcal_gadfs_vtmon_mif_table[target_mif_level_idx].vtmon_Cnt_Limit_set0);
			pwrcal_writel(VTMON1_CNT_LIMIT_SET1_SW0, pwrcal_gadfs_vtmon_mif_table[target_mif_level_idx].vtmon_Cnt_Limit_set1);

			//FSP OP=0, FSP-WR=1
			pwrcal_writel(DREX1_DIRECTCMD, (0x50 << 2) | (cur_drex_param[target_mif_level_idx].directCmdMr13));
			pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr1);
			pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr2);
			pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr3);
			pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr12);
			pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr14);
			pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr22);
			pwrcal_writel(DREX1_DIRECTCMD, cur_drex_param[target_mif_level_idx].directCmdMr11);
			/*		pwrcal_writel(DREX0_DIRECTCMD, cur_drex_param[target_mif_level_idx].drex_DirectCmd_mr14);	*/

			pwrcal_writel(VTMON1_DREX_TIMING_SW_SET, (0x1 << 0) | (cur_drex_param[target_mif_level_idx].timingSetSw));
			pwrcal_writel(VTMON1_MIF_DREX_WDATA0, (0xD0 << 2) | (cur_drex_param[target_mif_level_idx].directCmdMr13));


	} else {
		pr_err("wrong DMC timing set selection on DVFS\n");
		return;
	}
}

static unsigned int aref_upd_en;

void pwrcal_dmc_set_pre_dvfs(void)
{
	unsigned int uReg;
	int timeout;

	/*3. Disable VT MON Auto Refresh Update */
	aref_upd_en = pwrcal_readl(VTMON0_VT_MON_CON) & 0x30000000;

	uReg = pwrcal_readl(VTMON0_VT_MON_CON);
	uReg &= ~(3 << 28);
	pwrcal_writel(VTMON0_VT_MON_CON, uReg);
	for (timeout = 0;; timeout++) {
		if ((pwrcal_readl(VTMON0_PER_UPDATE_STATUS) & 0x1F) == 0x0)
			break;
		if (timeout > DMC_WAIT_CNT)
			return;
		cpu_relax();
	}

	uReg = pwrcal_readl(VTMON1_VT_MON_CON);
	uReg &= ~(3 << 28);
	pwrcal_writel(VTMON1_VT_MON_CON, uReg);
	for (timeout = 0;; timeout++) {
		if ((pwrcal_readl(VTMON1_PER_UPDATE_STATUS) & 0x1F) == 0x0)
			break;
		if (timeout > DMC_WAIT_CNT)
			return;
		cpu_relax();
	}
}

void pwrcal_dmc_set_post_dvfs(unsigned long long target_freq)
{
	unsigned int uReg;
	int timeout;

	/*2. Restore VT MON Auto Refresh Update */
	uReg = pwrcal_readl(VTMON0_VT_MON_CON);
	uReg |= aref_upd_en;
	pwrcal_writel(VTMON0_VT_MON_CON, uReg);
	for (timeout = 0;; timeout++) {
		if ((pwrcal_readl(VTMON0_PER_UPDATE_STATUS) & 0x1F) == 0x0)
			break;
		if (timeout > DMC_WAIT_CNT)
			return;
		cpu_relax();
	}

	uReg = pwrcal_readl(VTMON1_VT_MON_CON);
	uReg |= aref_upd_en;
	pwrcal_writel(VTMON1_VT_MON_CON, uReg);
	for (timeout = 0;; timeout++) {
		if ((pwrcal_readl(VTMON1_PER_UPDATE_STATUS) & 0x1F) == 0x0)
			break;
		if (timeout > DMC_WAIT_CNT)
			return;
		cpu_relax();
	}
}

void pwrcal_dmc_set_vtmon_on_swithing(void)
{
#if 0
	unsigned int uReg;
	int timeout;

	uReg = pwrcal_readl(VTMON0_VT_MON_CON);
	uReg |= (1 << 17);
	pwrcal_writel(VTMON0_VT_MON_CON, uReg);
	for (timeout = 0;; timeout++) {
		if ((pwrcal_readl(VTMON0_PER_UPDATE_STATUS) & 0x1F) == 0x0)
			break;
		if (timeout > DMC_WAIT_CNT)
			return;
		cpu_relax();
	}

	uReg = pwrcal_readl(VTMON1_VT_MON_CON);
	uReg |= (1 << 17);
	pwrcal_writel(VTMON1_VT_MON_CON, uReg);
	for (timeout = 0;; timeout++) {
		if ((pwrcal_readl(VTMON1_PER_UPDATE_STATUS) & 0x1F) == 0x0)
			break;
		if (timeout > DMC_WAIT_CNT)
			return;
		cpu_relax();
	}
#endif
}


void pwrcal_dmc_set_pbr(bool en)
{
	unsigned int uReg;

	uReg = pwrcal_readl(DREX0_MemControl);
	uReg = ((uReg & ~(1 << 27)) | (en << 27));
	pwrcal_writel(DREX0_MemControl, uReg);
	pwrcal_writel(DREX1_MemControl, uReg);
}

void pwrcal_dmc_set_refresh_method_pre_dvfs(unsigned long long current_rate, unsigned long long target_rate)
{
	/* target_rate is MIF clock rate */
	if (target_rate >= 1014 * MHZ)
		pwrcal_dmc_set_pbr(1);
	else
		pwrcal_dmc_set_pbr(0);
}

void pwrcal_dmc_set_refresh_method_post_dvfs(unsigned long long current_rate, unsigned long long target_rate)
{
	/* target_rate is MIF clock rate */
	if ((current_rate < 1014 * MHZ) && (target_rate >= 1014 * MHZ))
		pwrcal_dmc_set_pbr(1);
}

#if 0
void pwrcal_dmc_set_dsref_cycle(unsigned long long target_rate)
{
	unsigned int  uReg, cycle;

	/* target_rate is MIF clock rate */
	if (target_rate > 800 * MHZ)
		cycle = 0x3ff;
	else if (target_rate > 400 * MHZ)
		cycle = 0x1ff;
	else if (target_rate > 200 * MHZ)
		cycle = 0x90;
	else
		cycle = 0x90;


	/* dsref disable */
	uReg = pwrcal_readl(DREX0_MemControl);
	uReg &= ~(1 << 5);
	pwrcal_writel(DREX0_MemControl, uReg);
	pwrcal_writel(DREX1_MemControl, uReg);

	uReg = pwrcal_readl(DREX0_PwrDnConfig);
	uReg = ((uReg & ~(0xffff << 16)) | (cycle << 16));
	pwrcal_writel(DREX0_PwrDnConfig, uReg);
	pwrcal_writel(DREX1_PwrDnConfig, uReg);

	/* dsref enable */
	uReg = pwrcal_readl(DREX0_MemControl);
	uReg |= (1 << 5);
	pwrcal_writel(DREX0_MemControl, uReg);
	pwrcal_writel(DREX1_MemControl, uReg);

}
#endif

void pwrcal_dmc_set_clock_gating(int enable)
{
	unsigned int uReg;

	uReg = pwrcal_readl(DREX0_CgControl);
	uReg &= ~(1 << 4);
	uReg |= (enable << 4);
	pwrcal_writel(DREX0_CgControl, uReg);
	pwrcal_writel(DREX1_CgControl, uReg);
}

unsigned int pwrcal_dmc_get_pchannel_state(int channel)
{
	unsigned int uReg;

	if (channel == 0)
		uReg = pwrcal_readl(DREX0_Pchannel_Pause_State);
	else
		uReg = pwrcal_readl(DREX1_Pchannel_Pause_State);

	uReg = uReg>>24;
	uReg &= 0x7;

	return uReg;
}

unsigned int pwrcal_dmc_get_ptraing_state(void)
{
	unsigned int uReg;
	unsigned int state = 0;

	uReg = pwrcal_readl(DREX0_Hwpr_Train_Control_0);
	state = (uReg>>1)&1;

	uReg = pwrcal_readl(DREX0_Hwpr_Train_Control_1);
	state |= (uReg>>1)&1;

	return state;
}

void dfs_dram_param_init(void)
{
	int i;
	void *dram_block;
	int memory_size;
//	int memory_size = 0x10C; // means Samsung 12Gb Die
	struct ect_timing_param_size *size;
	void *loc;

	memory_size = pwrcal_readl(PMU_DREX_CALIBRATION3);

	if (memory_size == 0)
		memory_size = 0xC000501; // means Samsung 12Gb Die

	dram_block = ect_get_block(BLOCK_TIMING_PARAM);
	if (dram_block == NULL)
		return;

	size = ect_timing_param_get_size(dram_block, memory_size);
	if (size == NULL)
		return;

//	if (num_of_dram_parameter != size->num_of_timing_param)
//		return;

	pwrcal_gadfs_drex_mif_table = kzalloc(sizeof(dmc_drex_dfs_mif_table) * size->num_of_level, GFP_KERNEL);
	if (pwrcal_gadfs_drex_mif_table == NULL)
		return;

	pwrcal_gadfs_vtmon_mif_table = kzalloc(sizeof(dmc_vtmon_dfs_mif_table) * size->num_of_level, GFP_KERNEL);
	if (pwrcal_gadfs_vtmon_mif_table == NULL)
		return;

	pwrcal_gadfs_phy_mif_table = kzalloc(sizeof(dmc_phy_dfs_mif_table) * size->num_of_level, GFP_KERNEL);
	if (pwrcal_gadfs_phy_mif_table == NULL)
		return;

	loc =  size->timing_parameter;
	for (i = 0; i < size->num_of_level; ++i) {
		memcpy_toio(&pwrcal_gadfs_drex_mif_table[i], loc, sizeof(dmc_drex_dfs_mif_table));
		loc += sizeof(dmc_drex_dfs_mif_table);
		memcpy_toio(&pwrcal_gadfs_vtmon_mif_table[i], loc, sizeof(dmc_vtmon_dfs_mif_table));
		loc += sizeof(dmc_vtmon_dfs_mif_table);
		memcpy_toio(&pwrcal_gadfs_phy_mif_table[i], loc, sizeof(dmc_phy_dfs_mif_table));
		loc += sizeof(dmc_phy_dfs_mif_table);
	}
}

void dfs_mif_level_init(void)
{
	int i;
	void *dvfs_block;
	struct ect_dvfs_domain *domain;

	dvfs_block = ect_get_block(BLOCK_DVFS);
	if (dvfs_block == NULL)
		return;

	domain = ect_dvfs_get_domain(dvfs_block, vclk_dvfs_mif.vclk.name);
	if (domain == NULL)
		return;

	mif_freq_to_level = kzalloc(sizeof(unsigned long long) * domain->num_of_level, GFP_KERNEL);
	if (mif_freq_to_level == NULL)
		return;

	num_mif_freq_to_level = domain->num_of_level;

	for (i = 0; i < domain->num_of_level; ++i)
		mif_freq_to_level[i] = domain->list_level[i].level * KHZ;
}

void dfs_dram_init(void)
{
	dfs_dram_param_init();
	dfs_mif_level_init();
}
