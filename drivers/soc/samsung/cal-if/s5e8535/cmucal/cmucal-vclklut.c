#include "../../cmucal.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> LUT Parameter List */
unsigned int cmucal_VDDI_NM_lut_params[] = {
	0, 0,
};
unsigned int cmucal_VDDI_UD_lut_params[] = {
	1, 0,
};
unsigned int cmucal_VDDI_SUD_lut_params[] = {
	3, 0,
};
unsigned int cmucal_VDDI_UUD_lut_params[] = {
	4, 0,
};
unsigned int cmucal_VDD_ALIVE_SOD_lut_params[] = {
430000,430000,430000,430000,430000,27000,430000,430000,430000,430000,430000,215000,
};
unsigned int cmucal_VDD_ALIVE_OD_lut_params[] = {
430000,430000,430000,430000,430000,27000,430000,430000,430000,430000,430000,215000,
};
unsigned int cmucal_VDD_ALIVE_NM_lut_params[] = {
430000,430000,430000,430000,430000,27000,430000,430000,430000,430000,430000,215000,
};
unsigned int cmucal_VDD_ALIVE_UD_lut_params[] = {
430000,430000,430000,430000,430000,27000,430000,430000,430000,430000,430000,215000,
};
unsigned int cmucal_VDD_ALIVE_SUD_lut_params[] = {
430000,430000,430000,430000,430000,27000,430000,430000,430000,430000,430000,215000,
};
unsigned int cmucal_VDD_ALIVE_UUD_lut_params[] = {
430000,430000,430000,430000,430000,27000,430000,430000,430000,430000,430000,215000,
};
unsigned int cmucal_VDD_CHUB_SOD_lut_params[] = {
430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,
};
unsigned int cmucal_VDD_CHUB_OD_lut_params[] = {
430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,
};
unsigned int cmucal_VDD_CHUB_NM_lut_params[] = {
430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,
};
unsigned int cmucal_VDD_CHUB_UD_lut_params[] = {
430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,
};
unsigned int cmucal_VDD_CHUB_SUD_lut_params[] = {
430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,
};
unsigned int cmucal_VDD_CHUB_UUD_lut_params[] = {
430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,430000,
};
unsigned int cmucal_VDD_CPUCL0_SOD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,1800000,900000,600000,450000,52000,52000,1800000,1800000,1800000,1800000,1800000,900000,
};
unsigned int cmucal_VDD_CPUCL0_OD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,1500000,750000,500000,375000,52000,52000,1500000,1500000,1500000,1500000,1500000,750000,
};
unsigned int cmucal_VDD_CPUCL0_NM_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,1200000,600000,400000,300000,52000,52000,1200000,1200000,1200000,1200000,1200000,1200000,
};
unsigned int cmucal_VDD_CPUCL0_UD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,800000,400000,267000,200000,52000,52000,800000,800000,800000,800000,800000,800000,
};
unsigned int cmucal_VDD_CPUCL0_SUD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,400000,200000,134000,100000,52000,52000,400000,400000,400000,400000,400000,400000,
};
unsigned int cmucal_VDD_CPUCL0_UUD_lut_params[] = {
52000,52000,52000,52000,52000,52000,52000,52000,52000,52000,150000,75000,50000,38000,52000,52000,150000,150000,75000,150000,150000,150000,
};
unsigned int cmucal_VDD_CPUCL1_SOD_lut_params[] = {
2500000,1250000,834000,625000,52000,52000,200000,200000,200000,200000,
};
unsigned int cmucal_VDD_CPUCL1_OD_lut_params[] = {
2000000,1000000,667000,500000,52000,52000,200000,200000,200000,200000,
};
unsigned int cmucal_VDD_CPUCL1_NM_lut_params[] = {
1700000,850000,567000,425000,52000,52000,200000,200000,200000,200000,
};
unsigned int cmucal_VDD_CPUCL1_UD_lut_params[] = {
1100000,550000,367000,275000,52000,52000,200000,200000,200000,200000,
};
unsigned int cmucal_VDD_CPUCL1_SUD_lut_params[] = {
500000,250000,167000,125000,52000,52000,200000,200000,200000,200000,
};
unsigned int cmucal_VDD_CPUCL1_UUD_lut_params[] = {
500000,250000,167000,125000,52000,52000,200000,200000,200000,200000,
};
/* SPECIAL VCLK -> LUT Parameter List */
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_AUD_CPU_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL0_SWITCH_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_CPUCL1_SWITCH_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK0_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK1_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK2_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK3_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK4_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CIS_CLK5_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_TOP_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_CPU_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_CMU_BOOST_MIF_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_DPU_DSIM_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_HSI_UFS_EMBD_SOD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_HSI_UFS_EMBD_OD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_HSI_UFS_EMBD_NM_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_HSI_UFS_EMBD_UD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_HSI_UFS_EMBD_SUD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_HSI_UFS_EMBD_UUD_lut_params[] = {
    533,
};

unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_NOCL0_SC_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_PERI_MMC_CARD_SOD_lut_params[] = {
    809,
};
unsigned int cmucal_DIV_CLKCMU_PERI_MMC_CARD_OD_lut_params[] = {
    809,
};
unsigned int cmucal_DIV_CLKCMU_PERI_MMC_CARD_NM_lut_params[] = {
    809,
};
unsigned int cmucal_DIV_CLKCMU_PERI_MMC_CARD_UD_lut_params[] = {
    809,
};
unsigned int cmucal_DIV_CLKCMU_PERI_MMC_CARD_SUD_lut_params[] = {
    809,
};
unsigned int cmucal_DIV_CLKCMU_PERI_MMC_CARD_UUD_lut_params[] = {
    809,
};

unsigned int cmucal_DIV_CLKCMU_PERI_IP_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_OD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_NM_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_UD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_DIV_CLKCMU_PERI_IP_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_DIV_CLKCMU_USB_NOC_SOD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_OD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_NM_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_UD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_SUD_lut_params[] = {
    533,
};
unsigned int cmucal_DIV_CLKCMU_USB_NOC_UUD_lut_params[] = {
    533,
};

unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_OD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_NM_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_UD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_DIV_CLKCMU_USB_USB20DRD_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ALIVE_NOC_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_OD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_NM_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_UD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_SUD_lut_params[] = {
    666,
};
unsigned int cmucal_DIV_CLKCMU_CSIS_DCPHY_UUD_lut_params[] = {
    666,
};

unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_0_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_OD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_NM_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_UD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_SUD_lut_params[] = {
    800,
};
unsigned int cmucal_DIV_CLKCMU_ICPU_NOCD_1_UUD_lut_params[] = {
    800,
};

unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CMGP_SOD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CMGP_OD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CMGP_NM_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CMGP_UD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CMGP_SUD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CMGP_UUD_lut_params[] = {
    60,
};

unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CHUB_SOD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CHUB_OD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CHUB_NM_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CHUB_UD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CHUB_SUD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_CHUB_UUD_lut_params[] = {
    60,
};

unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_SPMI_SOD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_SPMI_OD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_SPMI_NM_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_SPMI_UD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_SPMI_SUD_lut_params[] = {
    60,
};
unsigned int cmucal_DIV_CLK_ALIVE_OSCCLK_RCO_SPMI_UUD_lut_params[] = {
    60,
};

unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_OD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_NM_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_UD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_PERI_ALIVE_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_OD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_NM_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_UD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CMGP_NOC_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_DIV_CLK_ALIVE_CHUB_NOC_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_NOC_OD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_NOC_NM_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_NOC_UD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_NOC_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_DIV_CLK_ALIVE_CHUB_NOC_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CMU_ALIVE_CLKOUT0_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CMU_ALIVE_CLKOUT0_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CMU_ALIVE_CLKOUT0_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CMU_ALIVE_CLKOUT0_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CMU_ALIVE_CLKOUT0_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CMU_ALIVE_CLKOUT0_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_IPCLKPORT_CLK_SOD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_IPCLKPORT_CLK_OD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_IPCLKPORT_CLK_NM_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_IPCLKPORT_CLK_UD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_IPCLKPORT_CLK_UUD_lut_params[] = {
    400,
};

unsigned int cmucal_CLK_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK_SOD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK_OD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK_NM_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK_UD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK_SUD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK_UUD_lut_params[] = {
    1200,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_SOD_lut_params[] = {
    300,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_OD_lut_params[] = {
    300,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_NM_lut_params[] = {
    300,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_UD_lut_params[] = {
    300,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_SUD_lut_params[] = {
    300,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF0_IPCLKPORT_CLK_UUD_lut_params[] = {
    300,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF1_IPCLKPORT_CLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF2_IPCLKPORT_CLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF3_IPCLKPORT_CLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF4_IPCLKPORT_CLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF5_IPCLKPORT_CLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_UAIF6_IPCLKPORT_CLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_DSIF_IPCLKPORT_CLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_SOD_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_OD_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_NM_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_UD_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_SUD_lut_params[] = {
    50,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_PCMC_IPCLKPORT_CLK_UUD_lut_params[] = {
    50,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_SOD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_OD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_NM_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_UD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACP_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_PCLKDBG_IPCLKPORT_CLK_SOD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_PCLKDBG_IPCLKPORT_CLK_OD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_PCLKDBG_IPCLKPORT_CLK_NM_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_PCLKDBG_IPCLKPORT_CLK_UD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_PCLKDBG_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_CPU_PCLKDBG_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK_SOD_lut_params[] = {
    533,
};
unsigned int cmucal_CLK_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK_OD_lut_params[] = {
    533,
};
unsigned int cmucal_CLK_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK_NM_lut_params[] = {
    533,
};
unsigned int cmucal_CLK_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK_UD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK_SUD_lut_params[] = {
    200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK_UUD_lut_params[] = {
    34,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_CORE_IPCLKPORT_CLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_CORE_IPCLKPORT_CLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_CORE_IPCLKPORT_CLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_CORE_IPCLKPORT_CLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_CORE_IPCLKPORT_CLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_CORE_IPCLKPORT_CLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_IPCLKPORT_CLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_IPCLKPORT_CLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_IPCLKPORT_CLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_IPCLKPORT_CLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_IPCLKPORT_CLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_SERIAL_LIF_IPCLKPORT_CLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK_SOD_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK_OD_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK_NM_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK_UD_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK_SUD_lut_params[] = {
    38,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK_UUD_lut_params[] = {
    38,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CNT_IPCLKPORT_CLK_SOD_lut_params[] = {
    600,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CNT_IPCLKPORT_CLK_OD_lut_params[] = {
    600,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CNT_IPCLKPORT_CLK_NM_lut_params[] = {
    600,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CNT_IPCLKPORT_CLK_UD_lut_params[] = {
    600,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CNT_IPCLKPORT_CLK_SUD_lut_params[] = {
    600,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CNT_IPCLKPORT_CLK_UUD_lut_params[] = {
    600,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_SOD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_OD_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_NM_lut_params[] = {
    1200,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_UD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_SR_CLK_AUD_CPU_ACLK_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_AUD_CODEC_MCLK_SOD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_AUD_CODEC_MCLK_OD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_AUD_CODEC_MCLK_NM_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_AUD_CODEC_MCLK_UD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_AUD_CODEC_MCLK_SUD_lut_params[] = {
    150,
};
unsigned int cmucal_CLK_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_AUD_CODEC_MCLK_UUD_lut_params[] = {
    150,
};

unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_SOD_lut_params[] = {
    25,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_OD_lut_params[] = {
    25,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_NM_lut_params[] = {
    25,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_UD_lut_params[] = {
    25,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_SUD_lut_params[] = {
    25,
};
unsigned int cmucal_CLK_BLK_AUD_UID_RSTnSYNC_CLK_AUD_FM_IPCLKPORT_CLK_UUD_lut_params[] = {
    25,
};

unsigned int cmucal_CLK_BLK_AUD_UID_ECU_AUD_IPCLKPORT_CLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_ECU_AUD_IPCLKPORT_CLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_ECU_AUD_IPCLKPORT_CLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_ECU_AUD_IPCLKPORT_CLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_ECU_AUD_IPCLKPORT_CLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_AUD_UID_ECU_AUD_IPCLKPORT_CLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI0_IPCLKPORT_CLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI0_IPCLKPORT_CLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI0_IPCLKPORT_CLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI0_IPCLKPORT_CLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI0_IPCLKPORT_CLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_SR_CLK_CHUB_USI0_IPCLKPORT_CLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_PCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_PCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_PCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_PCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_PCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_PCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_TIMER_IPCLKPORT_CLK_SOD_lut_params[] = {
    30,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_TIMER_IPCLKPORT_CLK_OD_lut_params[] = {
    30,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_TIMER_IPCLKPORT_CLK_NM_lut_params[] = {
    30,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_TIMER_IPCLKPORT_CLK_UD_lut_params[] = {
    30,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_TIMER_IPCLKPORT_CLK_SUD_lut_params[] = {
    30,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_RSTnSYNC_CLK_CHUB_TIMER_IPCLKPORT_CLK_UUD_lut_params[] = {
    30,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I2C_CHUB2_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CHUB_UID_I3C_CHUB_IPCLKPORT_i_sclk_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I3C_CHUB_IPCLKPORT_i_sclk_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I3C_CHUB_IPCLKPORT_i_sclk_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I3C_CHUB_IPCLKPORT_i_sclk_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I3C_CHUB_IPCLKPORT_i_sclk_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CHUB_UID_I3C_CHUB_IPCLKPORT_i_sclk_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D0_DPU_IPCLKPORT_PCLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D0_DPU_IPCLKPORT_PCLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D0_DPU_IPCLKPORT_PCLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D0_DPU_IPCLKPORT_PCLK_UD_lut_params[] = {
    444,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D0_DPU_IPCLKPORT_PCLK_SUD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_DPU_UID_PPMU_D0_DPU_IPCLKPORT_PCLK_UUD_lut_params[] = {
    67,
};

unsigned int cmucal_CLK_BLK_DPU_UID_RSTnSYNC_SR_CLK_DPU_ECU_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DPU_UID_RSTnSYNC_SR_CLK_DPU_ECU_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DPU_UID_RSTnSYNC_SR_CLK_DPU_ECU_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DPU_UID_RSTnSYNC_SR_CLK_DPU_ECU_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DPU_UID_RSTnSYNC_SR_CLK_DPU_ECU_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_DPU_UID_RSTnSYNC_SR_CLK_DPU_ECU_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_G3D_UID_PPMU_D_G3D_IPCLKPORT_PCLK_SOD_lut_params[] = {
    951,
};
unsigned int cmucal_CLK_BLK_G3D_UID_PPMU_D_G3D_IPCLKPORT_PCLK_OD_lut_params[] = {
    951,
};
unsigned int cmucal_CLK_BLK_G3D_UID_PPMU_D_G3D_IPCLKPORT_PCLK_NM_lut_params[] = {
    951,
};
unsigned int cmucal_CLK_BLK_G3D_UID_PPMU_D_G3D_IPCLKPORT_PCLK_UD_lut_params[] = {
    601,
};
unsigned int cmucal_CLK_BLK_G3D_UID_PPMU_D_G3D_IPCLKPORT_PCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_G3D_UID_PPMU_D_G3D_IPCLKPORT_PCLK_UUD_lut_params[] = {
    151,
};

unsigned int cmucal_CLK_BLK_G3D_UID_ECU_G3D_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_G3D_UID_ECU_G3D_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_G3D_UID_ECU_G3D_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_G3D_UID_ECU_G3D_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_G3D_UID_ECU_G3D_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_G3D_UID_ECU_G3D_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_HSI_UID_ECU_HSI_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_HSI_UID_ECU_HSI_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_HSI_UID_ECU_HSI_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_HSI_UID_ECU_HSI_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_HSI_UID_ECU_HSI_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_HSI_UID_ECU_HSI_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_MFC_UID_RSTnSYNC_CLK_MFC_NOCP_IPCLKPORT_CLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_MFC_UID_RSTnSYNC_CLK_MFC_NOCP_IPCLKPORT_CLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_MFC_UID_RSTnSYNC_CLK_MFC_NOCP_IPCLKPORT_CLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_MFC_UID_RSTnSYNC_CLK_MFC_NOCP_IPCLKPORT_CLK_UD_lut_params[] = {
    444,
};
unsigned int cmucal_CLK_BLK_MFC_UID_RSTnSYNC_CLK_MFC_NOCP_IPCLKPORT_CLK_SUD_lut_params[] = {
    333,
};
unsigned int cmucal_CLK_BLK_MFC_UID_RSTnSYNC_CLK_MFC_NOCP_IPCLKPORT_CLK_UUD_lut_params[] = {
    84,
};

unsigned int cmucal_CLK_BLK_MFC_UID_ECU_MFC_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MFC_UID_ECU_MFC_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MFC_UID_ECU_MFC_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MFC_UID_ECU_MFC_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MFC_UID_ECU_MFC_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_MFC_UID_ECU_MFC_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_SOD_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_OD_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_NM_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_UD_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_SUD_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF0_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_UUD_lut_params[] = {
    27,
};

unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_SOD_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_OD_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_NM_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_UD_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_SUD_lut_params[] = {
    27,
};
unsigned int cmucal_CLK_BLK_MIF1_UID_RSTnSYNC_SR_CLK_MIF_ECU_IPCLKPORT_CLK_UUD_lut_params[] = {
    27,
};

unsigned int cmucal_CLK_BLK_NOCL0_UID_AD_AXI_GIC_IPCLKPORT_ACLKM_SOD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_AD_AXI_GIC_IPCLKPORT_ACLKM_OD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_AD_AXI_GIC_IPCLKPORT_ACLKM_NM_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_AD_AXI_GIC_IPCLKPORT_ACLKM_UD_lut_params[] = {
    178,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_AD_AXI_GIC_IPCLKPORT_ACLKM_SUD_lut_params[] = {
    111,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_AD_AXI_GIC_IPCLKPORT_ACLKM_UUD_lut_params[] = {
    56,
};

unsigned int cmucal_CLK_BLK_NOCL0_UID_ECU_NOCL0_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_ECU_NOCL0_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_ECU_NOCL0_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_ECU_NOCL0_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_ECU_NOCL0_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL0_UID_ECU_NOCL0_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_NOCL1A_UID_ECU_NOCL1A_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL1A_UID_ECU_NOCL1A_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL1A_UID_ECU_NOCL1A_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL1A_UID_ECU_NOCL1A_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL1A_UID_ECU_NOCL1A_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_NOCL1A_UID_ECU_NOCL1A_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI00_USI_IPCLKPORT_CLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI00_USI_IPCLKPORT_CLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI00_USI_IPCLKPORT_CLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI00_USI_IPCLKPORT_CLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI00_USI_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI00_USI_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI01_USI_IPCLKPORT_CLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI01_USI_IPCLKPORT_CLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI01_USI_IPCLKPORT_CLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI01_USI_IPCLKPORT_CLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI01_USI_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI01_USI_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_USI02_USI_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI02_USI_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI02_USI_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI02_USI_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI02_USI_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI02_USI_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_USI03_USI_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI03_USI_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI03_USI_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI03_USI_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI03_USI_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI03_USI_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_IPCLKPORT_CLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_IPCLKPORT_CLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_IPCLKPORT_CLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_IPCLKPORT_CLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_USI05_USI_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI06_USI_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_UART_DBG_IPCLKPORT_CLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_UART_DBG_IPCLKPORT_CLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_UART_DBG_IPCLKPORT_CLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_UART_DBG_IPCLKPORT_CLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_UART_DBG_IPCLKPORT_CLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_RSTnSYNC_SR_CLK_PERI_UART_DBG_IPCLKPORT_CLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_USI05_I2C_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI05_I2C_IPCLKPORT_IPCLK_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI05_I2C_IPCLKPORT_IPCLK_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI05_I2C_IPCLKPORT_IPCLK_UD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI05_I2C_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_PERI_UID_USI05_I2C_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_PERI_UID_DFTMUX_PERIMMC_IPCLKPORT_I_CMU_CLKCMU_OTP_CLK_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_PERI_UID_DFTMUX_PERIMMC_IPCLKPORT_I_CMU_CLKCMU_OTP_CLK_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_PERI_UID_DFTMUX_PERIMMC_IPCLKPORT_I_CMU_CLKCMU_OTP_CLK_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_PERI_UID_DFTMUX_PERIMMC_IPCLKPORT_I_CMU_CLKCMU_OTP_CLK_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_PERI_UID_DFTMUX_PERIMMC_IPCLKPORT_I_CMU_CLKCMU_OTP_CLK_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_PERI_UID_DFTMUX_PERIMMC_IPCLKPORT_I_CMU_CLKCMU_OTP_CLK_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_RGBP_UID_PPMU_D1_RGBP_IPCLKPORT_PCLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_PPMU_D1_RGBP_IPCLKPORT_PCLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_PPMU_D1_RGBP_IPCLKPORT_PCLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_PPMU_D1_RGBP_IPCLKPORT_PCLK_UD_lut_params[] = {
    444,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_PPMU_D1_RGBP_IPCLKPORT_PCLK_SUD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_RGBP_UID_PPMU_D1_RGBP_IPCLKPORT_PCLK_UUD_lut_params[] = {
    267,
};

unsigned int cmucal_CLK_BLK_USB_UID_XIU_D_USB_IPCLKPORT_ACLK_SOD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_XIU_D_USB_IPCLKPORT_ACLK_OD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_XIU_D_USB_IPCLKPORT_ACLK_NM_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_XIU_D_USB_IPCLKPORT_ACLK_UD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_XIU_D_USB_IPCLKPORT_ACLK_SUD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_USB_UID_XIU_D_USB_IPCLKPORT_ACLK_UUD_lut_params[] = {
    267,
};

unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_SOD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_OD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_NM_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_UD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_SUD_lut_params[] = {
    26,
};
unsigned int cmucal_CLK_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26_UUD_lut_params[] = {
    26,
};

unsigned int cmucal_CLK_BLK_YUVP_UID_SLH_AXI_MI_P_YUVP_IPCLKPORT_I_CLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_SLH_AXI_MI_P_YUVP_IPCLKPORT_I_CLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_SLH_AXI_MI_P_YUVP_IPCLKPORT_I_CLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_SLH_AXI_MI_P_YUVP_IPCLKPORT_I_CLK_UD_lut_params[] = {
    444,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_SLH_AXI_MI_P_YUVP_IPCLKPORT_I_CLK_SUD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_YUVP_UID_SLH_AXI_MI_P_YUVP_IPCLKPORT_I_CLK_UUD_lut_params[] = {
    267,
};

unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_SOD_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_OD_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_NM_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_UD_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_SUD_lut_params[] = {
    167,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_RSTnSYNC_CLK_CSIS_DCPHY_IPCLKPORT_CLK_UUD_lut_params[] = {
    167,
};

unsigned int cmucal_CLK_BLK_CSIS_UID_QE_PDP_M0_IPCLKPORT_PCLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_PDP_M0_IPCLKPORT_PCLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_PDP_M0_IPCLKPORT_PCLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_PDP_M0_IPCLKPORT_PCLK_UD_lut_params[] = {
    444,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_PDP_M0_IPCLKPORT_PCLK_SUD_lut_params[] = {
    333,
};
unsigned int cmucal_CLK_BLK_CSIS_UID_QE_PDP_M0_IPCLKPORT_PCLK_UUD_lut_params[] = {
    333,
};

unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_CLK_CSTAT_NOCP_IPCLKPORT_CLK_SOD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_CLK_CSTAT_NOCP_IPCLKPORT_CLK_OD_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_CLK_CSTAT_NOCP_IPCLKPORT_CLK_NM_lut_params[] = {
    666,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_CLK_CSTAT_NOCP_IPCLKPORT_CLK_UD_lut_params[] = {
    444,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_CLK_CSTAT_NOCP_IPCLKPORT_CLK_SUD_lut_params[] = {
    333,
};
unsigned int cmucal_CLK_BLK_CSTAT_UID_RSTnSYNC_CLK_CSTAT_NOCP_IPCLKPORT_CLK_UUD_lut_params[] = {
    333,
};

unsigned int cmucal_CLK_BLK_M2M_UID_RSTnSYNC_SR_CLK_M2M_NOCP_IPCLKPORT_CLK_SOD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_M2M_UID_RSTnSYNC_SR_CLK_M2M_NOCP_IPCLKPORT_CLK_OD_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_M2M_UID_RSTnSYNC_SR_CLK_M2M_NOCP_IPCLKPORT_CLK_NM_lut_params[] = {
    800,
};
unsigned int cmucal_CLK_BLK_M2M_UID_RSTnSYNC_SR_CLK_M2M_NOCP_IPCLKPORT_CLK_UD_lut_params[] = {
    533,
};
unsigned int cmucal_CLK_BLK_M2M_UID_RSTnSYNC_SR_CLK_M2M_NOCP_IPCLKPORT_CLK_SUD_lut_params[] = {
    333,
};
unsigned int cmucal_CLK_BLK_M2M_UID_RSTnSYNC_SR_CLK_M2M_NOCP_IPCLKPORT_CLK_UUD_lut_params[] = {
    84,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_pclk_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_pclk_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_pclk_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_pclk_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_pclk_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_pclk_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK_UUD_lut_params[] = {
    430,
};

unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_SOD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_OD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_NM_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_UD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_SUD_lut_params[] = {
    430,
};
unsigned int cmucal_CLK_BLK_CMGP_UID_I3C_CMGP0_IPCLKPORT_i_sclk_UUD_lut_params[] = {
    430,
};


unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_SR_CLK_ICPU_NOCP_IPCLKPORT_CLK_SOD_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_SR_CLK_ICPU_NOCP_IPCLKPORT_CLK_OD_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_SR_CLK_ICPU_NOCP_IPCLKPORT_CLK_NM_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_SR_CLK_ICPU_NOCP_IPCLKPORT_CLK_UD_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_SR_CLK_ICPU_NOCP_IPCLKPORT_CLK_SUD_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_SR_CLK_ICPU_NOCP_IPCLKPORT_CLK_UUD_lut_params[] = {
    1334,
};

unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_CLK_ICPU_PCLKDBG_IPCLKPORT_CLK_SOD_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_CLK_ICPU_PCLKDBG_IPCLKPORT_CLK_OD_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_CLK_ICPU_PCLKDBG_IPCLKPORT_CLK_NM_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_CLK_ICPU_PCLKDBG_IPCLKPORT_CLK_UD_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_CLK_ICPU_PCLKDBG_IPCLKPORT_CLK_SUD_lut_params[] = {
    1334,
};
unsigned int cmucal_CLK_BLK_ICPU_UID_RSTnSYNC_CLK_ICPU_PCLKDBG_IPCLKPORT_CLK_UUD_lut_params[] = {
    1334,
};

unsigned int cmucal_CLK_BLK_S2D_UID_CMU_S2D_IPCLKPORT_PCLK_SOD_lut_params[] = {
    1,
};
unsigned int cmucal_CLK_BLK_S2D_UID_CMU_S2D_IPCLKPORT_PCLK_OD_lut_params[] = {
    1,
};
unsigned int cmucal_CLK_BLK_S2D_UID_CMU_S2D_IPCLKPORT_PCLK_NM_lut_params[] = {
    1,
};
unsigned int cmucal_CLK_BLK_S2D_UID_CMU_S2D_IPCLKPORT_PCLK_UD_lut_params[] = {
    1,
};
unsigned int cmucal_CLK_BLK_S2D_UID_CMU_S2D_IPCLKPORT_PCLK_SUD_lut_params[] = {
    1,
};
unsigned int cmucal_CLK_BLK_S2D_UID_CMU_S2D_IPCLKPORT_PCLK_UUD_lut_params[] = {
    1,
};

unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_SOD_lut_params[] = {
    2500,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_OD_lut_params[] = {
    2000,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_NM_lut_params[] = {
    1700,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_UD_lut_params[] = {
    1100,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_SUD_lut_params[] = {
    500,
};
unsigned int cmucal_BLK_CPUCL1_UID_CLUSTER0_IPCLKPORT_CORECLK_HC_UUD_lut_params[] = {
    500,
};

unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_SOD_lut_params[] = {
    1250,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_OD_lut_params[] = {
    1000,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_NM_lut_params[] = {
    850,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_UD_lut_params[] = {
    550,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_SUD_lut_params[] = {
    250,
};
unsigned int cmucal_CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK_UUD_lut_params[] = {
    250,
};

unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_SOD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_OD_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_NM_lut_params[] = {
    400,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_UD_lut_params[] = {
    267,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_SUD_lut_params[] = {
    200,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_SECJTAG_SM_IPCLKPORT_i_clk_UUD_lut_params[] = {
    100,
};

unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_SOD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_OD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_NM_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_UD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_SUD_lut_params[] = {
    52,
};
unsigned int cmucal_CLK_BLK_CPUCL0_GLB_UID_RSTnSYNC_SR_CLK_CPUCL0_GLB_ECU_IPCLKPORT_CLK_UUD_lut_params[] = {
    52,
};

/* PLL_SHARED0_D3(533MHz) DIV(2 + 1) */
unsigned int div_clk_hsi_ufs_embd_1776_params[] = {
	2, 2,
};
unsigned int div_clk_hsi_ufs_embd_1142_params[] = {
	6, 1,
};
unsigned int div_clk_peri_400_lut_params[] = {
	0, 1,
};
unsigned int div_clk_peri_200_lut_params[] = {
	1, 1,
};
unsigned int div_clk_peri_133_lut_params[] = {
	2, 1,
};
unsigned int div_clk_peri_100_lut_params[] = {
	3, 1,
};
unsigned int div_clk_peri_80_lut_params[] = {
	4, 1,
};
unsigned int div_clk_peri_66_lut_params[] = {
	5, 1,
};
unsigned int div_clk_peri_57_lut_params[] = {
	6, 1,
};
unsigned int div_clk_peri_50_lut_params[] = {
	7, 1,
};
unsigned int div_clk_peri_44_lut_params[] = {
	8, 1,
};
unsigned int div_clk_peri_40_lut_params[] = {
	9, 1,
};
unsigned int div_clk_peri_36_lut_params[] = {
	10, 1,
};
unsigned int div_clk_peri_33_lut_params[] = {
	11, 1,
};
unsigned int div_clk_peri_30_lut_params[] = {
	12, 1,
};
unsigned int div_clk_peri_28_lut_params[] = {
	13, 1,
};
unsigned int div_clk_peri_25_lut_params[] = {
	15, 1,
};
unsigned int div_clk_peri_26_lut_params[] = {
	0, 0,
};
unsigned int div_clk_peri_13_lut_params[] = {
	1, 0,
};
unsigned int div_clk_peri_8_lut_params[] = {
	2, 0,
};
unsigned int div_clk_peri_6_lut_params[] = {
	3, 0,
};
unsigned int div_clk_peri_5_lut_params[] = {
	4, 0,
};
unsigned int div_clk_peri_4_lut_params[] = {
	5, 0,
};
unsigned int div_clk_peri_3_lut_params[] = {
	6, 0,
};
